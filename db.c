#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

/* ssize_t from https://www.scivision.dev/ssize_t-visual-studio-posix/ */
#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

typedef struct
{
  char *buffer;
  size_t buffer_len;
  ssize_t input_len;
} InputBuffer;

typedef enum
{
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum
{
  PREPARE_SUCCESS,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_SYNTAX_ERROR,
  PREPARE_NEGATIVE_ID,
  PREPARE_STRING_TOO_LONG,
} PrepareResult;

typedef enum
{
  EXECUTE_SUCCESS,
  EXECUTE_TABLE_FULL
} ExecuteResult;

typedef enum
{
  STATEMENT_INSERT,
  STATEMENT_SELECT
} StatementType;

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

typedef struct
{
  uint32_t id;
  char username[COLUMN_USERNAME_SIZE + 1];
  char email[COLUMN_EMAIL_SIZE + 1];
} Row;

typedef struct
{
  StatementType type;
  Row row_to_insert;
} Statement;

#define size_of_attributes(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

const uint32_t ID_SIZE = size_of_attributes(Row, id);
const uint32_t USERNAME_SIZE = size_of_attributes(Row, username);
const uint32_t EMAIL_SIZE = size_of_attributes(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGE 100
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROW = ROWS_PER_PAGE * TABLE_MAX_PAGE;

typedef struct
{
  uint32_t num_rows;
  void *pages[TABLE_MAX_PAGE];
} Table;

ssize_t getline(char **lineptr, size_t *n, FILE *stream);
InputBuffer *new_input_buffer();
void print_prompt();
void read_input(InputBuffer *input_buffer);
void close_input(InputBuffer *input_buffer);
MetaCommandResult do_meta_command(InputBuffer *input_buffer, Table *table);
PrepareResult prepare_statement(InputBuffer *input_buffer, Statement *statement);
PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement);
ExecuteResult execute_statement(Statement *statement, Table *table);
Table *new_table();
void free_table(Table *table);
void print_row(Row *row);
ExecuteResult execute_insert(Statement *statement, Table *table);
ExecuteResult execute_select(Statement *statement, Table *table);
void seriealize_row(Row *source, void *destination);
void deserialize_row(void *source, Row *destination);
void *row_slot(Table *table, uint32_t row_num);

int main(int argc, char *argv[])
{
  Table *table = new_table();
  InputBuffer *input_buffer = new_input_buffer();
  while (1)
  {
    print_prompt();
    read_input(input_buffer);

    if (input_buffer->buffer[0] == '.')
    {
      switch (do_meta_command(input_buffer, table))
      {
      case (META_COMMAND_SUCCESS):
        continue;
      case (META_COMMAND_UNRECOGNIZED_COMMAND):
        printf("Unrecognized command '%s'\n", input_buffer->buffer);
        continue;
      default:
        continue;
      }
    }

    Statement statement;
    switch (prepare_statement(input_buffer, &statement))
    {
    case (PREPARE_SUCCESS):
      break;
    case (PREPARE_SYNTAX_ERROR):
      printf("Syntax error. Could not parse statement\n");
      continue;
    case (PREPARE_NEGATIVE_ID):
      printf("Id number can't be negative\n");
      continue;
    case (PREPARE_STRING_TOO_LONG):
      printf("String is too long\n");
      continue;
    case (PREPARE_UNRECOGNIZED_STATEMENT):
      printf("Unrecognized keyword at the start of '%s'\n", input_buffer->buffer);
      continue;
    default:
      continue;
    }

    switch (execute_statement(&statement, table))
    {
    case (EXECUTE_SUCCESS):
      printf("Executed\n");
      break;
    case (EXECUTE_TABLE_FULL):
      printf("Error: Table is full");
      break;
    default:
      break;
    }
  }
}

/* instantiate InputBuffer */
InputBuffer *new_input_buffer()
{
  InputBuffer *input_buffer = (InputBuffer *)malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_len = 0;
  input_buffer->input_len = 0;

  return input_buffer;
}

Table *new_table()
{
  Table *table = (Table *)malloc(sizeof(Table));
  table->num_rows = 0;
  for (size_t i = 0; i < TABLE_MAX_PAGE; i++)
  {
    table->pages[i] = NULL;
  }
  return table;
}

void free_table(Table *table)
{
  for (size_t i = 0; i < TABLE_MAX_PAGE; i++)
  {
    free(table->pages[i]);
  }
  free(table);
}

void print_row(Row *row)
{
  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

/* print prompt */
void print_prompt() { printf("DB> "); }

/* read input for InputBuffer struct */
void read_input(InputBuffer *input_buffer)
{
  ssize_t bytes_read = getline(&(input_buffer->buffer), &(input_buffer->buffer_len), stdin);

  if (bytes_read <= 0)
  {
    printf("Error reading input");
    exit(EXIT_FAILURE);
  }

  /* remove \n */
  input_buffer->input_len = bytes_read - 1;
  input_buffer->buffer[bytes_read - 1] = '\0';
}

/* clear InputBuffer struct and buffer member */
void close_input(InputBuffer *input_buffer)
{
  free(input_buffer->buffer);
  free(input_buffer);
}

MetaCommandResult do_meta_command(InputBuffer *input_buffer, Table *table)
{
  if (strcmp(input_buffer->buffer, ".exit") == 0)
  {
    close_input(input_buffer);
    free_table(table);
    exit(EXIT_SUCCESS);
  }
  else
  {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
}

/* prepare the statement */
PrepareResult prepare_statement(InputBuffer *input_buffer, Statement *statement)
{
  if (strncmp(input_buffer->buffer, "insert", 6) == 0)
  {
    return prepare_insert(input_buffer, statement);
  }
  /* not sure why this uses strcmp instead of strncmp */
  if (strcmp(input_buffer->buffer, "select") == 0)
  {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement)
{
  statement->type = STATEMENT_INSERT;
  char *keyword = strtok(input_buffer->buffer, " ");
  char *id_string = strtok(NULL, " ");
  char *username = strtok(NULL, " ");
  char *email = strtok(NULL, " ");

  if (id_string == NULL || username == NULL || email == NULL)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  int id = atoi(id_string);
  if (id < 0)
  {
    return PREPARE_NEGATIVE_ID;
  }

  if (strlen(username) > COLUMN_USERNAME_SIZE || strlen(email) > COLUMN_EMAIL_SIZE)
  {
    return PREPARE_STRING_TOO_LONG;
  }

  statement->row_to_insert.id = id;
  strcpy(statement->row_to_insert.username, username);
  strcpy(statement->row_to_insert.email, email);

  return PREPARE_SUCCESS;
}

/* function for executing statement */
ExecuteResult execute_statement(Statement *statement, Table *table)
{
  switch (statement->type)
  {
  case (STATEMENT_INSERT):
    return execute_insert(statement, table);
  case (STATEMENT_SELECT):
    return execute_select(statement, table);
  default:
    break;
  }
}

ExecuteResult execute_insert(Statement *statement, Table *table)
{
  if (table->num_rows >= TABLE_MAX_ROW)
  {
    return EXECUTE_TABLE_FULL;
  }
  seriealize_row(&(statement->row_to_insert), row_slot(table, table->num_rows));
  table->num_rows += 1;

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table)
{
  Row row;
  for (size_t i = 0; i < table->num_rows; i++)
  {
    deserialize_row(row_slot(table, i), &row);
    print_row(&row);
  }
  return EXECUTE_SUCCESS;
}

void seriealize_row(Row *source, void *destination)
{
  memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
  memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
  memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void *source, Row *destination)
{
  memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
  memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
  memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

void *row_slot(Table *table, uint32_t row_num)
{
  uint32_t page_num = row_num / ROWS_PER_PAGE;
  void *page = table->pages[page_num];
  if (page == NULL)
  {
    page = table->pages[page_num] = malloc(PAGE_SIZE);
  }
  uint32_t row_offset = row_num % ROWS_PER_PAGE;
  uint32_t byte_offset = row_offset * ROW_SIZE;
  return page + byte_offset;
}

/* getline function from stackoverflow https://stackoverflow.com/questions/735126/are-there-alternate-implementations-of-gnu-getline-interface/735472#735472 */
ssize_t getline(char **line_ptr, size_t *buffer_len_ptr, FILE *stream)
{
  size_t pos;
  int c;

  if (line_ptr == NULL || stream == NULL || buffer_len_ptr == NULL)
  {
    errno = EINVAL;
    return -1;
  }

  /* get the first character of input */
  c = getc(stream);

  /* skip over all the initial whitespace */
  /* while ((c = getc(stream)) == ' '); */

  if (c == EOF)
  {
    return -1;
  }

  if (*line_ptr == NULL)
  {
    *line_ptr = malloc(128);
    if (*line_ptr == NULL)
    {
      return -1;
    }
    *buffer_len_ptr = 128;
  }

  pos = 0;
  while (c != EOF)
  {
    if (pos + 1 >= *buffer_len_ptr)
    {
      size_t new_size = *buffer_len_ptr + (*buffer_len_ptr >> 2);
      if (new_size < 128)
      {
        new_size = 128;
      }
      char *new_ptr = realloc(*line_ptr, new_size);
      if (new_ptr == NULL)
      {
        return -1;
      }
      *buffer_len_ptr = new_size;
      *line_ptr = new_ptr;
    }

    ((unsigned char *)(*line_ptr))[pos++] = c;
    if (c == '\n')
    {
      break;
    }
    c = getc(stream);
  }

  (*line_ptr)[pos] = '\0';
  return pos;
}