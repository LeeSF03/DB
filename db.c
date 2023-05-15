#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

typedef struct
{
  char *buffer;
  size_t buffer_len;
  ssize_t input_len;

} InputBuffer;

ssize_t getline(char **lineptr, size_t *n, FILE *stream);
InputBuffer* new_input_buffer ();
void print_prompt();
void read_input(InputBuffer *input_buffer);
void close_input(InputBuffer *input_buffer);

int main(int argc, char *argv[])
{
  InputBuffer *input_buffer = new_input_buffer();
  while (1)
  {
    print_prompt();
    read_input(input_buffer);

    if (strcmp(input_buffer->buffer, ".exit") == 0)
    {
      close_input(input_buffer);
      exit(EXIT_SUCCESS);
    } else
    {
      printf("Unrecognized command '%s'. \n", input_buffer->buffer);
    }
  }
}

/* instantiate InputBUffer */
InputBuffer* new_input_buffer () {
  InputBuffer* input_buffer = (InputBuffer *)malloc(sizeof(InputBuffer));
  input_buffer->buffer = NULL;
  input_buffer->buffer_len = 0;
  input_buffer->input_len = 0;

  return input_buffer;
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
void close_input(InputBuffer *input_buffer) {
  free(input_buffer->buffer);
  free(input_buffer);
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

  c = getc(stream);
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