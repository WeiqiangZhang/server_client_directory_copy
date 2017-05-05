#include <stdio.h>
#include "ftree.h"
#ifndef PORT
  #define PORT 30000
#endif
int main(){
    fcopy_server(PORT);
    return 0;
}