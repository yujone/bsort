#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


#define SWITCH_TO_SHELL 20


int verbosity;


struct sort {
  int fd;
  off_t size;
  void *buffer;
};


static unsigned long getTick(void) {
  struct timespec ts;
  unsigned theTick = 0U;
  clock_gettime( CLOCK_REALTIME, &ts );
  theTick  = ts.tv_nsec / 1000000;
  theTick += ts.tv_sec * 1000;
  return theTick;
}

// bubble sort
static inline void
shellsort(unsigned char *a,
          const int n,
          const int record_size,
          const int key_size) {
  int i, j;
  char temp[record_size];

  for (i=3; i < n; i++) {
	//Bubble Sort at 3 lines intervals
    memcpy(&temp, &a[i * record_size], record_size);
    for(j=i; j>=3 && memcmp(a+(j-3)*record_size, &temp, key_size) >0; j -= 3) {
      memcpy(a+j*record_size, a+(j-3)*record_size, record_size);
    }
    memcpy(a+j*record_size, &temp, record_size);
  }

  for (i=1; i < n; i++) {
	//Bubble Sort at one line intervals
    memcpy(&temp, &a[i*record_size], record_size);
    for(j=i; j>=1 && memcmp(a+(j-1)*record_size, &temp, key_size) >0; j -= 1) {
      memcpy(a+j*record_size, a+(j-1)*record_size, record_size);
    }
    memcpy(a+j*record_size, &temp, record_size);
  }
}


int compare(int *length, unsigned char *a, unsigned char *b) {
  return memcmp(a, b, *length);
}


void
radixify(unsigned char *buffer,
         const long count,
         const long digit,
         const long char_start,
         const long char_stop,
         const long record_size,
         const long key_size,
         const long stack_size,
         const long cut_off) {
  long counts[char_stop+1];
  long offsets[char_stop+1];
  long starts[char_stop+1];
  long ends[char_stop+1];
  long offset=0;
  unsigned char temp[record_size];
  long target, x, a, b;
  long stack[stack_size];
  long stack_pointer;
  long last_position, last_value, next_value;

  /*
  initial buffer

  version https://
  git-lfs.github.c
  om/spec/v1oid s
  ha256:692c5672ce
  7b764a6260ce7fd9
  44098bf345823775
  4b35ac5119fe3ac6
  2c4b44size 1311
  0\n


  count = filesize(130 byte)/record_size(10 byte) = 8

  on the first invocation

  char_start = 0
  char_stop  = 255

  char_start and char_stop is the start and end line of sorted group by first digit chars
  in recursive invocation


  key_size   = 10(default)
  stack_size = 5(default)

  */



  if (verbosity && digit == 0)
    fprintf(stderr, "radixify(count=%ld, digit=%ld, char_start=%ld, char_stop=%ld, record_size=%ld, key_size=%ld, stack_size=%ld, cut_off=%ld)\n", count, digit, char_start, char_stop, record_size, key_size, stack_size, cut_off);

  // initialize counts and offsets array
  for (x=char_start; x<=char_stop; x++) {
    counts[x] = 0;
    offsets[x] = 0;
  }

  // Compute starting positions
  // count the number of data at byte offset digit in each line group by Ascii code
  for (x=0; x<count; x++) {
	// c = Ascii code
    long c = buffer[x*record_size + digit];

    // count the number of each Ascii code
    counts[c] += 1;
  }

  /*
	counts[50]	1		HEX:32  Ascii:2
	counts[52]	2		HEX:34  Ascii:4
	counts[55]	1		HEX:37  Ascii:7
	counts[103]	1	    HEX:67  Ascii:g
	counts[104]	1		HEX:68  Ascii:h
	counts[111]	1		HEX:6F  Ascii:o
	counts[118]	1		HEX:76  Ascii:v

   */


  // Compute offsets(start line of each Ascii code in sorted array)
  // according to each Ascii code count
  // ends[x] - starts[x] = Ascii code x count
  offset = 0;
  for(x=char_start; x<=char_stop; x++) {
    offsets[x] = offset;
    starts[x] = offsets[x];
    offset += counts[x];
  }

  for(x=char_start; x<char_stop; x++) {
    ends[x] = offsets[x+1];
  }
  ends[char_stop] = count;

/*

  counts[x] =  ends[50] -  starts[x]

  starts[50]		0		HEX:32				ends[50]    1		Ascii:2
  starts[51]		1							ends[51]    1
  starts[52]		1		HEX:34	CNT 2		ends[52]    3		Ascii:4
  starts[53]		3							ends[53]    3
  starts[54]		3							ends[54]    3
  starts[55]		3		HEX:37				ends[55]    4		Ascii:7
  starts[56]		4							ends[56]    4
  starts[102]		4							ends[102]   4
  starts[103]		4	    HEX:67				ends[103]   5		Ascii:g
  starts[104]		5		HEX:68				ends[104]   6		Ascii:h
  starts[105]		6							ends[105]   6
  starts[110]		6							ends[110]   6
  starts[111]		6		HEX:6F				ends[111]   7		Ascii:o
  starts[112]		7							ends[112]   7
  starts[117]		7							ends[117]   7
  starts[118]		7		HEX:76				ends[118]   8		Ascii:v
  starts[119]		8							ends[119]   8

  char_start <= the range of starts <= char_stop
  at first offsets array = starts array

  char_start <= the range of ends   <= char_stop
  ends[x] = offsets[x+1]

*/


  for(x=char_start; x<=char_stop; x++) {

	//check that Ascii code x is all sorted
	// offsetx[x] == ends[x] when Ascii code x is all sorted
    while (offsets[x] < ends[x]) {

      if (buffer[offsets[x] * record_size + digit] == x) {
    	// buffer[offsets[x] * record_size + digit] is Ascii code x
    	// and don't need move position
    	// just offsets[x]++
        offsets[x] += 1;
      } else {
        stack_pointer=0;
        // stack is the line offset
        stack[stack_pointer] = offsets[x];
        stack_pointer += 1;

        // target is Ascii code in the target line
        target = buffer[offsets[x] * record_size + digit];


        while( target != x && stack_pointer < stack_size ) {

          // the src line will be moved to offset line of the target Ascii code in src line
          // and offsets[target]++
          // if the target Ascii code isn't x
          stack[stack_pointer] = offsets[target];
          offsets[target] += 1;

          //get the Ascii code in the next target line
          target = buffer[stack[stack_pointer] * record_size + digit];
          stack_pointer++;
        };


        if (stack_pointer != stack_size) {
          offsets[x] += 1;
        }
/*
        initial buffer			offset line

        version https://		0
        git-lfs.github.c		1
        om/spec/v1oid s			2
        ha256:692c5672ce		3
        7b764a6260ce7fd9		4
        44098bf345823775		5
        4b35ac5119fe3ac6		6
        2c4b44size 1311			7
        0\n

		x='2'

		stack[0] =  offsets['2']	=	1		target = 'v'
		stack[1] =  offsets['v']	=	7		target = '2'  						offsets['v'] += 1 == 8
		target == x								stack_pointer = 2 != stack_size(5) 	offsets['2'] += 1 == 1

		memcpy(&temp, the last line in stack);
		whiel memcpy(the stack_pointer line in stack, the (stack_pointer - 1) line in stack );
		memcpyy(the first line in stack, the last line in stack);

        2c4b44size 1311			0
        git-lfs.github.c		1
        om/spec/v1oid s			2
        ha256:692c5672ce		3
        7b764a6260ce7fd9		4
        44098bf345823775		5
        4b35ac5119fe3ac6		6
		version https://		7
        0\n

		x='4'
		stack[0] =  offsets['4']	=	1		target = 'g'
		stack[1] =  offsets['g']	=	4		target = '7'  						offsets['g'] += 1 == 5
		stack[2] =  offsets['7']	=	3		target = 'h'  						offsets['7'] += 1 == 4
		stack[3] =  offsets['h']	=	5		target = '4'  						offsets['h'] += 1 == 6
		target == x								stack_pointer = 4 != stack_size(5) 	offsets['4'] += 1 == 2

		memcpy(&temp, line5);
		memcp(line5,  line3)
		memcp(line3,  line4)
		memcp(line4,  line1)
		memcp(line1,  &temp)

		2c4b44\nsize 1311		0
		44098bf345823775		1
		om/spec/v1\noid s		2
		7b764a6260ce7fd9		3
		git-lfs.github.c		4
		ha256:692c5672ce		5
		4b35ac5119fe3ac6		6
		version https://		7
		0\n


		( offsets['4'] == 2 )< ( ends['4'] == 3 )

		stack[0] =  offsets['4']	=	2		target = 'o'
		stack[1] =  offsets['o']	=	6		target = '4'  						offsets['o'] += 1 == 7
		target == x								stack_pointer = 2 != stack_size(5) 	offsets['4'] += 1 == 3


		2c4b44\nsize 1311		0
		44098bf345823775		1
		4b35ac5119fe3ac6		2
		7b764a6260ce7fd9		3
		git-lfs.github.c		4
		ha256:692c5672ce		5
		om/spec/v1\noid s		6
		version https://		7
		0\n

*/

        stack_pointer--;
        memcpy(&temp, &buffer[stack[stack_pointer] * record_size], record_size);
        while (stack_pointer) {
          memcpy(&buffer[stack[stack_pointer] * record_size], &buffer[stack[stack_pointer-1] * record_size], record_size);
          stack_pointer--;
        }
        memcpy(&buffer[stack[0] * record_size], &temp, record_size);
      }
    }
  }

  if (digit < cut_off) {
    for(x=char_start; x<=char_stop; x++) {
      if ( ends[x] - starts[x] > SWITCH_TO_SHELL) {
        radixify(&buffer[starts[x] * record_size],
                 ends[x] - starts[x],
                 digit+1,
                 char_start,
                 char_stop,
                 record_size,
                 key_size,
                 stack_size,
                 cut_off);
      } else {
        if (ends[x] - starts[x] <= 1) continue;
        shellsort(&buffer[starts[x] * record_size], ends[x] - starts[x], record_size, key_size);
        //qsort_r(&buffer[starts[x] * record_size], ends[x] - starts[x], record_size, &compare, &record_size);
      }
    }
  } else {
    for(x=char_start; x<=char_stop; x++)
      if (ends[x] - starts[x] > 1) {
        shellsort(&buffer[starts[x] * record_size], ends[x] - starts[x], record_size, key_size);
        //qsort_r(&buffer[starts[x] * record_size], ends[x] - starts[x], record_size, &compare, &record_size);
      }
  }
}


int open_sort(char *path, struct sort *sort) {
  void *buffer = NULL;

  int fd = open(path, O_RDWR);
  if (fd == -1)
    goto error;

  struct stat stats;
  if (-1 == fstat(fd, &stats))
    goto error;
  if (!(buffer = mmap(NULL,
                      stats.st_size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      fd,
                      0
                     )))
    goto error;


  madvise(buffer, stats.st_size, POSIX_MADV_WILLNEED | POSIX_MADV_SEQUENTIAL);

  sort->buffer = buffer;
  sort->size = stats.st_size;
  sort->fd = fd;
  return 0;

error:
  perror(path);
  if (buffer)
    munmap(buffer, stats.st_size);
  if (fd != -1)
    close(fd);
  sort->buffer = 0;
  sort->fd = 0;
  return -1;
}


void close_sort(struct sort *sort) {
  if (sort->buffer) {
    munmap(sort->buffer, sort->size);
    sort->buffer = 0;
    sort->size = 0;
  }

  if (sort->fd) {
    close(sort->fd);
    sort->fd = 0;
  }
}


int
main(int argc, char *argv[]) {
  int opt;
  int char_start = 0;
  int char_stop = 255;
  int record_size=100;
  int key_size=10;
  int stack_size=5;
  int cut_off = 4;
  verbosity = 0;

  while ((opt = getopt(argc, argv, "var:k:s:c:")) != -1) {
    switch (opt) {
    case 'v':
      verbosity += 1;
      break;
    case 'a':
      // Ascii code start and stop
      char_start = 32;
      char_stop = 128;
      break;
    case 'r':
      // record length(byte)
      record_size = atoi(optarg);
      break;
    case 'k':
      // sort key length(byte)
      key_size = atoi(optarg);
      break;
    case 's':
      // buffer size which is used in radixify
      stack_size = atoi(optarg);
      break;
    case 'c':
      // first cut_off bytes of each record use radixify  to sort
      // after cut_off byte  of each record use shellsort to sort
      cut_off = atoi(optarg);
    default:
      fprintf(stderr, "Invalid parameter: -%c\n", opt);
      goto failure;
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Expected argument after options\n");
    goto failure;
  }

  unsigned long TickStart = getTick();

  while(optind < argc) {
    if (verbosity)
      printf("sorting %s\n", argv[optind]);
    struct sort sort;
    if (-1==open_sort(argv[optind], &sort))
      goto failure;

    radixify(sort.buffer,
             sort.size / record_size,
             0,
             char_start,
             char_stop,
             record_size,
             key_size,
             stack_size,
             cut_off);
    close_sort(&sort);
    optind++;
  }

  printf("Processing time: %.3f s\n", (float)(getTick() - TickStart) / 1000);

  exit(0);
failure:
  fprintf(stderr,
          "Usage: %s [-v] [-a] [-r ###] [-k ###] [-s ###] file1, file2 ... \n",
          argv[0]);
  fprintf(stderr,
          "Individually sort binary files inplace with a radix sort\n"
          "\n"
          "Sorting Options:\n"
          "\n"
          "  -a       assume files are printable 7-bit ascii instead of binary\n"
          "  -k ###   size of compariable section of record, in bytes (default 100)\n"
          "  -r ###   size of overall record, in bytes.  (default 100)\n"
          "\n"
          "Options:\n"
          "  -v  verbose output logging\n"
          "\n"
          "Tuning Options:\n"
          "\n"
          "  -s ###   pushahead stack size.  (default 12)\n"
          "  -c ###   recursion limit after which to use shell sort (defaults to 4)\n"
          "\n"
          "Report bsort bugs to adam@pelotoncycle.com\n"
         );
  exit(1);
}




