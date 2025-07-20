#include "tinyexpr.h"
#include <stdio.h>

int main() {
 const char *expression = "2 + 3 * sin(pi/2)";
 int error;
 double result = te_interp(expression, &error);

 if (error == 0) {
     printf("Result: %f\n", result);  /* Output: Result: 5.000000 */
 } else {
     printf("Error at position %d\n", error);
 }

 return 0;
}