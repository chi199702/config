#include <stdio.h>

/* bool value */
#define TRUE  1
#define FALSE 0

/* option size */
#define OPTION_BUFFER_SIZE 1024
#define ITEM_BUFFER_SIZE   1024
#define PRINT_BUFFER       1024

/* type of value */
#define TYPE_UNVALID (0)
#define TYPE_NULL    (1 << 0)
#define TYPE_FALSE   (1 << 1)
#define TYPE_TRUE    (1 << 2)
#define TYPE_NUMBER  (1 << 3)
#define TYPE_STRING  (1 << 4)

/* the configration structure */
typedef struct Config {
  char* name;
  struct Item* child;
  struct Config* prev;  /* point the last Config in the chain */
  struct Config* next;
}Config;

/* indicate a setting option */
typedef struct Item {
  int type;
  char* key;
  int value_int;
  double value_double;
  char* value_string; 
  struct Item* prev;
  struct Item* next;
}Item;

typedef int BOOL;

/* parse a config file and get a config* pointer 
 * return TRUE(1) while parse success, otherwise return FALSE(0)
 * */
BOOL parse(Config* const config, const char* const path);

/* get [key, value] though key 
 * return non-NULL while [key, value] is exist, otherwise return NULL, if exist multiple same key [key, value], return the first meet
 * */
Item* get_item(const Config* const config, const char* const key);

/* print the list */
void print(const Config* const config);

/* free Config* */
void free_all(Config* config);
