#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#include "config.h"

static Item* parse_item(Config* config, const char* const buffer);
static BOOL  parse_with_file_pointer(Config* const config, FILE* file_pointer);
static Item* find_pair(const Config* const config, const char* const key);
static void  free_item(Item* item);
static void  free_config(Config* config);

/* skip blank, must push back buffer if the last character is not blank, string starting with # will be considered as comment */
static void skip_blank_and_comment(FILE* file_pointer) {
  int c;
  while (TRUE) {
    while (!feof(file_pointer) && (c = fgetc(file_pointer)) <= 32); /* skip blank */
    if (!feof(file_pointer)) {
      ungetc(c, file_pointer);
    }
    if ((c = fgetc(file_pointer)) == '#') {
      while (!feof(file_pointer) && (c = fgetc(file_pointer)) != '\n'); /* skip comment */
    }else {
      ungetc(c, file_pointer);
      break;
    }
  }
}

/* check if space left of buffer > offset */
static BOOL can_read(const char* const buffer, const char* const cursor, unsigned int offset) {
  if (cursor + offset - buffer < strlen(buffer)) {
    return TRUE;
  }
  return FALSE;
}

/* create a new Config* and add it to the end of the link list */
static Config* create_config(const Config* const config) {
  Config* head_config = (Config*)config;
  if (head_config == NULL) {
    if ((head_config = (Config*)malloc(sizeof(Config))) == NULL) {
      fprintf(stderr, "error occur in function create_config : malloc config failed!");
      return NULL;
    }
    memset((void*)head_config, 0, sizeof(Config));
    head_config -> prev = head_config;
    return head_config;
  }

  Config* last_config = head_config -> prev;  /* remember : the first config prev point the last config in chain */
  Config* new_config = NULL;
  if ((new_config = (Config*)malloc(sizeof(Config))) == NULL) {
    fprintf(stderr, "error occur in function create_Config : malloc Config failed!");
    return NULL;
  }
  memset((void*)new_config, 0, sizeof(Config));
  new_config -> prev = last_config;
  last_config -> next = new_config;
  head_config -> prev = new_config;
  return new_config;
}

/* create a item* and add it to the end of the child link list */
static Item* create_item(const Config* const config) {
  if (config == NULL) {
    fprintf(stderr, "null pointer exception occur in function create_item");
    return NULL;
  }
  Config* current_config = (Config*)config;
  while (current_config -> next != NULL) {
    current_config = current_config -> next;
  }
  Item* head_item = current_config -> child;
  Item* new_item = NULL;
  if (head_item == NULL) {
    if ((new_item = (Item*)malloc(sizeof(Item))) == NULL) {
      fprintf(stderr, "error occur int function create_item : malloc item failed");
      return NULL;
    }
    memset((void*)new_item, 0, sizeof(Item));
    new_item -> prev = new_item;  /* the first item of link list, it pointer prev point the last_item, that can get the last_item quickly */
    return new_item;
  }
  if ((new_item = (Item*)malloc(sizeof(Item))) == NULL) {
    fprintf(stderr, "error occur int function create_item : malloc item failed");
    return NULL;
  }
  memset((void*)new_item, 0, sizeof(Item));
  Item* last_item = head_item -> prev;
  last_item -> next = new_item;
  new_item  -> prev = last_item;
  head_item -> prev = new_item;
  return new_item;
}

/* parse a [string], the string will be add in Config* -> name */
static Config* add_config(Config* const config, FILE* file_pointer) {
  if (config == NULL || file_pointer == NULL) {
    fprintf(stderr, "null pointer exception occur in function add_Config");
    return NULL;
  }
  int c;
  if (feof(file_pointer) || (c = fgetc(file_pointer)) != '[') { /* end of file or not [] */
    ungetc(c, file_pointer);
    return NULL;
  }

  char buffer[OPTION_BUFFER_SIZE];
  memset((void*)buffer, '\0', OPTION_BUFFER_SIZE);
  unsigned long buffer_length = strlen(buffer);
  while (!feof(file_pointer) && buffer_length < OPTION_BUFFER_SIZE - 1 && (c = fgetc(file_pointer)) != -1 && c != ']') {
    if (c <= 32 || !isalnum(c)) {
      fprintf(stderr, "illeagl input in [], the character in [] must be digit or english letters and is not blank");
      return NULL;
    }
    buffer[buffer_length++] = c;
  }
  buffer[buffer_length] = '\0';

  if (c != ']' || !buffer_length) { /* parse [] error */
    fprintf(stderr, "illeagl input in [], may be missing ] or [] is blank or the length of [] string > 1024");
    return NULL;
  }
  Config* new_config = create_config(config);
  buffer_length = strlen(buffer);
  if ((new_config -> name = (char*)malloc(sizeof(char) * (buffer_length + 1))) == NULL) {
    fprintf(stderr, "error occur in function add_config : malloc char failed!");
  }
  strcpy(new_config -> name, buffer);
  return new_config;
}

/* parse a item, the item will be added to the end of child link list */
static Item* add_item(Config* const config, FILE* file_pointer) {
  if (config == NULL || file_pointer == NULL) {
    fprintf(stderr, "null pointer exception occur in function add_Item");
    return NULL;
  }
  int c;
  char buffer[ITEM_BUFFER_SIZE];
  memset((void*)buffer, '\0', ITEM_BUFFER_SIZE);
  unsigned long buffer_length;
  fgets(buffer, ITEM_BUFFER_SIZE, file_pointer);
  buffer_length = strlen(buffer);
  buffer[buffer_length] = '\0'; /* replace with '\n' */
  return parse_item(config, buffer); /* parse_item function will parse the item and add it to the end of config */ 
}

/*
 * parse a item
 * return non-zero if success, otherwise return NULL
 * */
static Item* parse_item(Config* config, const char* const buffer) {
  if (!strlen(buffer)) {
    return NULL;
  }
  
  ptrdiff_t key_length = 0;
  Item* new_item = NULL;
  char* key_end = (char*)buffer;
  
  if ((new_item = create_item(config)) == NULL) {
    return NULL;
  }

  while (isalnum(*key_end++)); 
  --key_end;
  char* cursor = key_end--;
  while (*cursor != '\0' && *cursor != '=') { /* check if exist is not character or digit between key and '=' */
    if (*cursor == '#') { /* meet comment, should stop parse */
      break;
    } 
    if (*cursor != ' ') {
      fprintf(stderr, "illeagl input of item key, a item is string = value, string must be english character or number!");
      return NULL;
    }
    ++cursor;
  }

  /* copy key to new_item -> key */
  key_length = key_end - buffer + 1;
  if ((new_item -> key = (char*)malloc(sizeof(char) * (key_length + 1))) == NULL) { 
    fprintf(stderr, "error occur in function parse_item : malloc (item -> key) falied!");
    return NULL;
  }
  strncpy(new_item -> key, buffer, key_length);
  (new_item -> key)[key_length] = '\0';

  if (*cursor == '\0' || *cursor == '#') {  /* item only has key */
    return new_item;
  }

  ++cursor; /* skip '=' */
  while (*cursor != '\0' && *cursor++ == ' ');  /* skip blank ' ' */
  if (*cursor == '#') {
    new_item -> type = TYPE_NULL;
    return new_item;
  }

  switch (*cursor) {
    /* null or NULL */
    case 'n' : case 'N' : {
      if (can_read(buffer, cursor, 3) && (!strncmp(cursor, "null", 4) || !strncmp(cursor, "NULL", 4))) {
        new_item -> type = TYPE_NULL;
        cursor += 4;
      }
      break;
    }
    /* digit */
    case '-' : case '+' : case '0' : case '1' : case '2' : case '3' : case '4' : case '5' : case '6' : case '7' : case '8' : case '9' : {
      int i = 0;
      char digit[32];
      if (*cursor == '+' || *cursor == '-') {
        digit[i++] = *cursor++;
      }
      while (isdigit(*cursor) && i < 31) {
        digit[i++] = *cursor++;
      }
      digit[i] = '\0';
      new_item -> type = TYPE_NUMBER;
      new_item -> value_double = atof(digit);
      new_item -> value_int = (int)new_item -> value_double;
      break;
    }
    /* string */
    case '\"': {
      int i = 0;
      char string_buffer[ITEM_BUFFER_SIZE];
      
      while (*cursor != '\0' && *cursor != '\"' && i < ITEM_BUFFER_SIZE - 1) {
        string_buffer[i++] = *cursor++;
      }
      if (*cursor == '\0' || i == ITEM_BUFFER_SIZE - 1) {
        fprintf(stderr, "illeagl input of item string, may be lost \" or the length is too long!");
        return NULL;
      }
      string_buffer[i] = '\0';
      ++cursor; /* skip '\"' */
      unsigned int string_buffer_length = strlen(string_buffer);
      if ((new_item -> value_string = (char*)malloc(sizeof(char) * (string_buffer_length + 1))) == NULL) {
        fprintf(stderr, "error occur in function parse_item : malloc item -> value_string failed!");
        return NULL;
      }
      strcpy(new_item -> value_string, string_buffer);
      new_item -> type = TYPE_STRING;
      break;
    }
    /* true or TRUE */
    case 't' : case 'T' : {
      if (can_read(buffer, cursor, 3) && (!strncmp(cursor, "true", 4) || !strncmp(cursor, "TRUE", 4))) {
        new_item -> type = TYPE_TRUE;
        cursor += 4;
      }
      break;
    }
    /* false or FALSE */
    case 'f' : case 'F' : {
      if (can_read(buffer, cursor, 3) && (!strncmp(cursor, "false", 5) || !strncmp(cursor, "FALSE", 5))) {
        new_item -> type = TYPE_FALSE;
        cursor += 5;
      }
      break;
    }
    /* invalid */
    default:
      fprintf(stderr, "error occur in function parse_item, illeagl input of item value!");
      return NULL;
  }
  /* skip blank */
  while (*cursor++ == ' ');
  --cursor;
  /* after parse item value complete, only comments are allowed to appear */
  if (*cursor != '#' || *cursor != '\0') {
    fprintf(stderr, "error occur in function parse_item, illeagl input of item value!");
  }
  return new_item;
}


BOOL parse(Config* const config, const char* const path) {
  FILE* file_pointer = NULL;
  if ((file_pointer = fopen(path, "r")) == NULL) {
    perror("config.c : function parse_with_file_pointer");
    return FALSE;
  }
  if (parse_with_file_pointer(config, file_pointer)) {
    return TRUE;
  }
  return FALSE;
}

/* return TRUE(1) while parse success,otherwise return FALSE(0) */
static BOOL parse_with_file_pointer(Config* const config, FILE* file_pointer) {
  skip_blank_and_comment(file_pointer);

  int c;
  while (!feof(file_pointer) && (c = fgetc(file_pointer)) != -1) {
    if (c == '[') { /* meet a [] */
      ungetc(c, file_pointer);
      if(!add_config(config, file_pointer)) {
        free_all(config);
        return FALSE;
      }
      skip_blank_and_comment(file_pointer);
    }else { /* meet a item */
      ungetc(c, file_pointer);
      if (!add_item(config, file_pointer)) {
        free_all(config);
        return FALSE;
      }
      skip_blank_and_comment(file_pointer);
    }
  }
  return TRUE;
}

Item* get_pair(const Config* const config, const char* const key) {
  if (config == NULL || key == NULL) {
    return NULL;
  } 
  return find_pair(config, key); 
}

static Item* find_pair(const Config* const config, const char* const key) {
  Item* item = NULL;
  Config* current_config = (Config*)config;  

  while (current_config) {
    /* find the item of current_config */
    {
      item = current_config -> child; 
      while (item) {
        if (!strcmp(key, item -> key)) {
          return item;
        }
        item = item -> next;
      }
    }
    current_config = current_config -> next;
  }
  return NULL;
}

void print(const Config* const config) {
  char buffer[PRINT_BUFFER];
}

/* free Config* */
void free_all(Config* config) {
  free_config(config); 
}

/* free a config recursively */
static void free_config(Config* config) {
  if (!config) {
    return;
  } 
  if (config -> next) {
    free_config(config -> next);
  }else {
    free(config -> name);
    free_item(config -> child);
    config -> name = NULL;
  }
}

/* free a item recursively */
static void free_item(Item* item) {
  if (!item) {
    return;
  }
  if (item -> next) {
    free_item(item -> next);
  }else {
    free(item -> key);
    free(item -> value_string);
    item -> key = NULL;
    item -> value_string = NULL;
  }
}
