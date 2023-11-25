#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>

int jmpfunction();
void jmpfunction2(jmp_buf);

int main () {
   int res = jmpfunction();
   printf("res = %d\n", res);
   
   return(0);
}

int jmpfunction() {
   jmp_buf env_buffer;

   /* save calling environment for longjmp */
   int val = setjmp(env_buffer);
   if (val == 0) {
      printf("setjmp\n");
      jmpfunction2(env_buffer);
   } else {
      printf("Returned from a longjmp() with value = %d\n", val);
   }

   return val;
}

void jmpfunction2(jmp_buf env_buffer) {
   printf("In jmpfunction2\n");
   longjmp(env_buffer, 1);
}
