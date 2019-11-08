#include <iostream>
#include <stdio.h>
#include <string.h>

using namespace std;

#define MEM_SIZE 8192
#define MAX_LINES (MEM_SIZE/2)

typedef struct LINE {
    bool visited;
    bool decoded;
    char text[32];
} LINE_t;

static uint8_t mem_byte[MEM_SIZE];
static int dump_size;
static uint16_t *cmd = reinterpret_cast<uint16_t*>(mem_byte);
static LINE_t line[MAX_LINES];
static int pc;

static char hex2val(char c)
{
    if( c >= '0' && c <= '9' )
        return c - '0';
    else if( c >= 'A' && c <= 'F' )
        return 0x0A + c - 'A';
    return 0;
}

static uint8_t hex2byte(const char *hex)
{
    return uint8_t((hex2val(hex[0]) << 4) + hex2val(hex[1]));
}

static uint16_t hex2word(const char *hex)
{
    return uint16_t((hex2byte(hex) << 8) + hex2byte(&hex[2]));
}

static void print_damp()
{
    for( int i = 0; i < dump_size; i++ )
    {
        printf("%02X", mem_byte[i]);
        if( i % 16 == 15 || i == dump_size - 1) puts("");
    }
}

static void print_code()
{
    for( int i = 0; i < MAX_LINES; i++ )
        if( line[i].decoded )
            puts(line[i].text);
}

static void load_hex(const char *file_name)
{
    FILE *fhex;
    fhex = fopen(file_name, "rt");
    if( fhex != nullptr )
    {
        puts("File opened:");
        char hex_line[256];
        memset(mem_byte, 0xff, MEM_SIZE );
        dump_size = 0;
        while( fgets(hex_line, 255, fhex) )
        {
            int hex_len = int(strlen(hex_line)) - 1;
            hex_line[hex_len] = 0;
            if(   hex_line[0] == ':'
               && hex_len > 11
               && (hex_len & 1)
               && hex2byte(&hex_line[7]) == 0 )
            {
                uint8_t  size = hex2byte(&hex_line[1]);
                uint16_t addr = hex2word(&hex_line[3]);
                for( uint8_t i = 0; i < size; i++ )
                    mem_byte[addr+i] = hex2byte(&hex_line[9+i*2]);
                if( dump_size < addr + size )
                    dump_size = addr + size;
            }
        }
        fclose(fhex);
    }
    else
    {
        puts("Can't open\n");
    }
}

typedef bool (*COMMAND_t)(void);

static bool cmd_rjmp_rcall()
{
    if( (cmd[pc] & 0xC000) != 0xC000)
        return false;
    strcpy(line[pc].text, "rjmp");
    return true;
}

static COMMAND_t command[] = {
    cmd_rjmp_rcall
};

#define COMMAND_COUNT (sizeof(command)/sizeof(COMMAND_t))

static void decode_chain()
{
    for(unsigned i = 0; i < COMMAND_COUNT; i++)
    {
        int addr = pc;
        if( command[i]() )
        {
            line[addr].decoded = true;
            line[addr].visited = true;
            break;
        }
    }
}


int main()
{
    cout << "Hello World!\n" << endl;
    load_hex("GPig.hex");
    print_damp();
    pc = 0;
    decode_chain();
    print_code();
    return 0;
}
