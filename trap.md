# Trap Answer & Note
## Question
## 1. RISC-V assembly
1. Which registers contain arguments to functions? For example, which register holds 13 in main's call to printf?
- Ans: a2, 可以由 `li   a2,13`得知，這是直接展開的結果。
2. Where is the call to function f in the assembly code for main? Where is the call to g? (Hint: the compiler may inline functions.)
- Ans: NAN, 其實不存在，compiler將結果直接展開成 `li	a1,12`，因此，我們實際上看不到function call of f(), 因為直接被inline掉了。
3. At what address is the function printf located?
- Ans: 0x6f6 `00000000000006f6 <printf>:`
4. What value is in the register ra just after the jalr to printf in main?
- Ans: 這題跳過，不存在jalr的指令。
5. 
```
Run the following code.What is the output?

	unsigned int i = 0x00646c72;
	printf("H%x Wo%s", 57616, (char *) &i);
```
- Ans: `HE110 World`,  (1) 57616 -> 0xE110, (2) 可視為將u32強轉型成字串，低位元到高位元是：0x72:r , 0x6c:l, 0x64:d,0x00(最高位元):'\0',恰為中止字元。
6. In the following code, what is going to be printed after 'y='? (note: the answer is not a specific value.) Why does this happen?
```
printf("x=%d y=%d", 3);
```
- Ans:a2的殘留數值，屬於undefined behavior,a0放的是printf字串位址，a1是第一個%d得值，a2是第二個%d的數值，由於未提供這個參數，故會直接是原本的殘留資訊。補充一點：a0會是`addi	a0,a0,-1912 # 8b0 <malloc+0x106>`其實就是指向.rodata(存放字串的地方)，帶由於objdump只會出現.text的資訊，因此以最接近的點作為參考，故會被誤導好像是在malloc裡面，實際不是，實際上應該是在.rodata的字段了。

- notice:
    - s0: callee saved register, 通常也會用於frame pointer使用。在這個call.c裡面就是如此。

