#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "net.h"

// xv6's ethernet and IP addresses
static uint8 local_mac[ETHADDR_LEN] = { 0x52, 0x54, 0x00, 0x12, 0x34, 0x56 };
static uint32 local_ip = MAKE_IP_ADDR(10, 0, 2, 15);

// qemu host's ethernet address.
static uint8 host_mac[ETHADDR_LEN] = { 0x52, 0x55, 0x0a, 0x00, 0x02, 0x02 };

static struct spinlock netlock;

#define RECV_BUF_SIZE 8
struct netbuf_t {
  uint32 saddr;
  uint16 sport;
  int len;
  int rdbytes; // user might not read all data at once, so rdlen records
               // accumulate bytes the user has taken.
  char *data;
  char *raw_buf; // use for kfree
};

struct netbufqueue {
  uint8 head;
  uint8 tail;
  struct netbuf_t netbuf[RECV_BUF_SIZE];
};

struct dport_netbuque {
  int dport; // -1 as not register
  struct netbufqueue netbuf_queue;
};

struct dport_chansignal {
  int dport; // -1 as not use
  uint chan;
};

struct dport_chansignal dp_chansigs[MAXNETPORT];
struct dport_netbuque dport_nbqs[MAXNETPORT];
int g_bindedports[MAXNETPORT]; // register all processes' binded port

static void netbufque_init() {
  memset(dport_nbqs, 0, sizeof(dport_nbqs));
  for (int i = 0; i < MAXNETPORT; i++) {
    dport_nbqs[i].dport = -1;
  }
}

// to deal with reading half buffer situation, NBQ should first get, if all data
// has been read, and then do pop. Get func won't push tail index!
static void netbufque_pop(struct dport_netbuque *dport_nbq) {
  struct netbufqueue *nbq = &dport_nbq->netbuf_queue;

  if (nbq->tail == nbq->head) {
    printf("pop: netbufque empty\n");
    return;
  }
  nbq->tail = (nbq->tail + 1) % RECV_BUF_SIZE;
  return;
}
// to deal with reading half buffer situation, NBQ should first get, if all data
// has been read, and then do pop. Get func won't push tail index!
static struct netbuf_t *netbufque_get(struct dport_netbuque *dport_nbq) {
  struct netbufqueue *nbq = &dport_nbq->netbuf_queue;
  struct netbuf_t *netbuf;
  if (nbq->tail == nbq->head) {
    printf("get: netbufque empty\n");
    return (struct netbuf_t *)0;
  }
  netbuf = &nbq->netbuf[nbq->tail];
  return netbuf;
}

static void netbufque_push(struct dport_netbuque *dport_nbq,
                           struct netbuf_t *netbuf) {
  struct netbufqueue *nbq = &dport_nbq->netbuf_queue;
  if (((nbq->head + 1) % RECV_BUF_SIZE) == nbq->tail) {
    printf("netbufque full, drop buf\n");
    kfree(netbuf->raw_buf);
    return;
  }
  memmove(&nbq->netbuf[nbq->head], netbuf, sizeof(struct netbuf_t));
  nbq->head = (nbq->head + 1) % RECV_BUF_SIZE;
  return;
}

static int netbufque_isempty(struct dport_netbuque *dport_nbq) {
  return (dport_nbq->netbuf_queue.head == dport_nbq->netbuf_queue.tail);
}

// portregister 1: register 0: unregister dport_nbq
static int set_dport_nbq(int dport, int portregister) {

  for (int i = 0; i < MAXNETPORT; i++) {
    struct dport_netbuque *dport_nbq = &dport_nbqs[i];
    if (dport_nbq->dport == -1 && portregister == 1) {
      dport_nbq->dport = dport;
      return 0;
    } else if (dport_nbq->dport == dport && portregister == 0) {
      dport_nbq->dport = -1;
      printf("dport_nbq: unregister success\n");
      return 0;
    }
  }
  if (portregister)
    printf("dport_nbq is full!\n");
  else
    printf("dport_nbq is empty!\n");
  return -1;
}

static struct dport_netbuque *get_dport_nbq(int dport) {
  for (int i = 0; i < MAXNETPORT; i++) {
    struct dport_netbuque *dport_nbq = &dport_nbqs[i];
    if (dport == dport_nbq->dport) {
      return dport_nbq;
    }
  }
  printf("target port(%d) not binded!\n", dport);
  return (struct dport_netbuque *)0;
}

static void *getchanflag(int dport) {
  for (int i = 0; i < MAXNETPORT; i++) {
    if (dp_chansigs[i].dport == dport) {
      dp_chansigs[i].dport = -1;
      return (void *)&dp_chansigs[i].chan;
    }
  }
  return (void *)0;
}
static void *setchanflag(int dport) {
  for (int i = 0; i < MAXNETPORT; i++) {
    if (dport == dp_chansigs[i].dport) {
      printf("regchan: duplicating register same port(%d)!", dport);
      return (void *)0;
    }
  }
  for (int i = 0; i < MAXNETPORT; i++) {
    if (dp_chansigs[i].dport == -1) {
      dp_chansigs[i].dport = dport;
      return (void *)&dp_chansigs[i].chan;
    }
  }
  return (void *)0;
}

void netinit(void) {
  initlock(&netlock, "netlock");
  netbufque_init();
  for (int i = 0; i < MAXNETPORT; i++) {
    g_bindedports[i] = -1;
    dp_chansigs[i].dport = -1;
  }
}

//
// bind(int port)
// prepare to receive UDP packets address to the port,
// i.e. allocate any queues &c needed.
//
uint64 sys_bind(void) {
  int port;
  argint(0, &port);

  for (int i = 0; i < MAXNETPORT; i++) {
    if (g_bindedports[i] == -1) {
      g_bindedports[i] = port;
      // printf("bind success, idx=%d, port=%d\n", i, port);
      return set_dport_nbq(port, 1);
    }
  }
  printf("bindports have been all occupied!\n");
  return -1;
}

//
// unbind(int port)
// release any resources previously created by bind(port);
// from now on UDP packets addressed to port should be dropped.
//
uint64 sys_unbind(void) {
  int port;
  argint(0, &port);

  for (int i = 0; i < MAXNETPORT; i++) {
    if (g_bindedports[i] == port) {
      g_bindedports[i] = -1;
      break;
    }
  }
  return set_dport_nbq(port, 0);
}

//
// recv(int dport, int *src, short *sport, char *buf, int maxlen)
// if there's a received UDP packet already queued that was
// addressed to dport, then return it.
// otherwise wait for such a packet.
//
// sets *src to the IP source address.
// sets *sport to the UDP source port.
// copies up to maxlen bytes of UDP payload to buf.
// returns the number of bytes copied,
// and -1 if there was an error.
//
// dport, *src, and *sport are host byte order.
// bind(dport) must previously have been called.
//
uint64 sys_recv(void) {

  struct proc *p = myproc();
  uint64 usrcaddr;
  uint64 usportaddr;
  int umaxlen;
  int udport;
  uint64 ubufaddr;
  struct netbuf_t *netbuf;
  int rdlenmax;

  argint(0, &udport);
  argaddr(1, &usrcaddr);
  argaddr(2, &usportaddr);
  argaddr(3, &ubufaddr);
  argint(4, &umaxlen);

  struct dport_netbuque *dport_nbq = get_dport_nbq(udport);
  if (dport_nbq == (struct dport_netbuque *)0)
    panic("net_recv: dport not find"); // TODO: change to return -1
  acquire(&netlock);
  while (netbufque_isempty(dport_nbq)) { // avoid fake wakeup signal
    void *pchan = setchanflag(udport);
    sleep(pchan, &netlock);
    getchanflag(udport); // clear pchan, if drop happens, it might occur fake
                         // duplicate register warning
  }
  netbuf = netbufque_get(dport_nbq);

  if (copyout(p->pagetable, usrcaddr, (char *)&netbuf->saddr, sizeof(uint32)) <
      0)
    return -1;

  if (copyout(p->pagetable, usportaddr, (char *)&netbuf->sport,
              sizeof(uint16)) < 0)
    return -1;

  rdlenmax = (netbuf->len > umaxlen) ? umaxlen : netbuf->len;
  if (copyout(p->pagetable, ubufaddr, (char *)netbuf->data, rdlenmax) < 0)
    return -1;

  release(&netlock);
  netbufque_pop(dport_nbq); // TODO: buffer read all check
  kfree(netbuf->raw_buf);
  return rdlenmax;
}

// This code is lifted from FreeBSD's ping.c, and is copyright by the Regents
// of the University of California.
static unsigned short
in_cksum(const unsigned char *addr, int len)
{
  int nleft = len;
  const unsigned short *w = (const unsigned short *)addr;
  unsigned int sum = 0;
  unsigned short answer = 0;

  /*
   * Our algorithm is simple, using a 32 bit accumulator (sum), we add
   * sequential 16 bit words to it, and at the end, fold back all the
   * carry bits from the top 16 bits into the lower 16 bits.
   */
  while (nleft > 1)  {
    sum += *w++;
    nleft -= 2;
  }

  /* mop up an odd byte, if necessary */
  if (nleft == 1) {
    *(unsigned char *)(&answer) = *(const unsigned char *)w;
    sum += answer;
  }

  /* add back carry outs from top 16 bits to low 16 bits */
  sum = (sum & 0xffff) + (sum >> 16);
  sum += (sum >> 16);
  /* guaranteed now that the lower 16 bits of sum are correct */

  answer = ~sum; /* truncate to 16 bits */
  return answer;
}

//
// send(int sport, int dst, int dport, char *buf, int len)
//
uint64
sys_send(void)
{
  struct proc *p = myproc();
  int sport;
  int dst;
  int dport;
  uint64 bufaddr;
  int len;

  argint(0, &sport);
  argint(1, &dst);
  argint(2, &dport);
  argaddr(3, &bufaddr);
  argint(4, &len);

  int total = len + sizeof(struct eth) + sizeof(struct ip) + sizeof(struct udp);
  if(total > PGSIZE)
    return -1;

  char *buf = kalloc();
  if(buf == 0){
    printf("sys_send: kalloc failed\n");
    return -1;
  }
  memset(buf, 0, PGSIZE);

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, host_mac, ETHADDR_LEN);
  memmove(eth->shost, local_mac, ETHADDR_LEN);
  eth->type = htons(ETHTYPE_IP);

  struct ip *ip = (struct ip *)(eth + 1);
  ip->ip_vhl = 0x45; // version 4, header length 4*5
  ip->ip_tos = 0;
  ip->ip_len = htons(sizeof(struct ip) + sizeof(struct udp) + len);
  ip->ip_id = 0;
  ip->ip_off = 0;
  ip->ip_ttl = 100;
  ip->ip_p = IPPROTO_UDP;
  ip->ip_src = htonl(local_ip);
  ip->ip_dst = htonl(dst);
  ip->ip_sum = in_cksum((unsigned char *)ip, sizeof(*ip));

  struct udp *udp = (struct udp *)(ip + 1);
  udp->sport = htons(sport);
  udp->dport = htons(dport);
  udp->ulen = htons(len + sizeof(struct udp));

  char *payload = (char *)(udp + 1);
  if(copyin(p->pagetable, payload, bufaddr, len) < 0){
    kfree(buf);
    printf("send: copyin failed\n");
    return -1;
  }

  e1000_transmit(buf, total);

  return 0;
}

void
ip_rx(char *buf, int len)
{
  // don't delete this printf; make grade depends on it.
  static int seen_ip = 0;
  if(seen_ip == 0)
    printf("ip_rx: received an IP packet\n");
  seen_ip = 1;

  struct eth *eth = (struct eth *)buf;
  // no check here, hardware will take care of mac filter

  struct ip *ip = (struct ip *)(eth + 1);
  uint32 ip_src = ntohl(ip->ip_src);
  if (((ip->ip_vhl & 0xF0) >> 4) != 4)
    panic("ip: version error");
  if ((uint8)(ip->ip_vhl & 0x0F) != 5)
    panic("ip: header length error");
  if (in_cksum((unsigned char *)ip, sizeof(*ip)))
    panic("ip: checksum error");

  struct udp *udp = (struct udp *)(ip + 1);
  uint16 dport = ntohs(udp->dport);
  uint16 sport = ntohs(udp->sport);
  // if (in_cksum((unsigned char *)udp, ntohs(udp->ulen)) && udp->sum)
  //   panic("udp: checksum error"); // optional? not forcing in UDP

  uint16 datalen = ntohs(udp->ulen) - sizeof(*udp);
  char *data = (char *)(udp + 1);

  // put data to queue
  struct dport_netbuque *nbq = get_dport_nbq(dport);
  if (nbq == (struct dport_netbuque *)0) {
    printf("ip_rx: no proc is waiting this message, drop buf\n");
    kfree(buf);
    return;
  }
  // printf("iprx: push to nbq(%p), dport(%d)\n", nbq, nbq->dport);
  struct netbuf_t netbuf = {
      .data = data,
      .len = datalen,
      .rdbytes = 0,
      .saddr = ip_src,
      .sport = sport,
      .raw_buf = buf,
  };
  netbufque_push(nbq, &netbuf);
  void *pchan = getchanflag(dport);
  if (pchan != (void *)0) {
    wakeup(pchan);
  }
}

//
// send an ARP reply packet to tell qemu to map
// xv6's ip address to its ethernet address.
// this is the bare minimum needed to persuade
// qemu to send IP packets to xv6; the real ARP
// protocol is more complex.
//
void
arp_rx(char *inbuf)
{
  static int seen_arp = 0;

  if(seen_arp){
    kfree(inbuf);
    return;
  }
  printf("arp_rx: received an ARP packet\n");
  seen_arp = 1;

  struct eth *ineth = (struct eth *) inbuf;
  struct arp *inarp = (struct arp *) (ineth + 1);

  char *buf = kalloc();
  if(buf == 0)
    panic("send_arp_reply");

  struct eth *eth = (struct eth *) buf;
  memmove(eth->dhost, ineth->shost, ETHADDR_LEN); // ethernet destination = query source
  memmove(eth->shost, local_mac, ETHADDR_LEN); // ethernet source = xv6's ethernet address
  eth->type = htons(ETHTYPE_ARP);

  struct arp *arp = (struct arp *)(eth + 1);
  arp->hrd = htons(ARP_HRD_ETHER);
  arp->pro = htons(ETHTYPE_IP);
  arp->hln = ETHADDR_LEN;
  arp->pln = sizeof(uint32);
  arp->op = htons(ARP_OP_REPLY);

  memmove(arp->sha, local_mac, ETHADDR_LEN);
  arp->sip = htonl(local_ip);
  memmove(arp->tha, ineth->shost, ETHADDR_LEN);
  arp->tip = inarp->sip;

  e1000_transmit(buf, sizeof(*eth) + sizeof(*arp));

  kfree(inbuf);
}

void
net_rx(char *buf, int len)
{
  struct eth *eth = (struct eth *) buf;

  if(len >= sizeof(struct eth) + sizeof(struct arp) &&
     ntohs(eth->type) == ETHTYPE_ARP){
    arp_rx(buf);
  } else if(len >= sizeof(struct eth) + sizeof(struct ip) &&
     ntohs(eth->type) == ETHTYPE_IP){
    ip_rx(buf, len);
  } else {
    kfree(buf);
  }
}
