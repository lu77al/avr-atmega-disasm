#include <iostream>
#include <stdio.h>
#include <string.h>
#include "math_utils.h"

using namespace std;

#define MEM_SIZE 8192
#define MAX_LINES (MEM_SIZE/2)
#define MAX_ORIGINS MAX_LINES

typedef struct LINE {
    bool visited;
    bool decoded;
    bool pointed;
    char text[32];
} LINE_t;

static uint8_t mem_byte[MEM_SIZE];
static int dump_size;
static uint16_t *code = reinterpret_cast<uint16_t*>(mem_byte);
static LINE_t line[MAX_LINES];
static uint16_t pc;
static uint16_t origin[MAX_ORIGINS];
static uint16_t origin_cnt;

typedef bool (*COMMAND_t)(void);

//----------------------------------------------------------------------
static void add_origin(uint16_t addr)
{
    origin[origin_cnt++] = addr;
}

//----------------------------------------------------------------------
static void delete_first_origin()
{
    if( origin_cnt > 0 )
    {
        origin_cnt--;
        memmove(origin, &origin[1], origin_cnt*sizeof(int) );
    }
}

//----------------------------------------------------------------------
static bool cmd_rjmp_rcall()
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xE000) != 0xC000)
        return false;
    uint16_t addr;
    if( BIT(cmd, 11) )
        addr = pc + 1 - (0x1000 - F16(cmd, 0, 12) );
    else
        addr = pc + 1 + F16(cmd, 0, 12);
    line[addr].pointed = true;
    if( BIT(cmd, 12) )
        sprintf(line[pc].text, "rcall\tL%X", addr);
    else
        sprintf(line[pc].text, "rjmp\tL%X", addr);
    pc = addr - 1;
    return true;
}

//----------------------------------------------------------------------
static bool cmd_ldi()
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xF000) != 0xE000)
        return false;
    uint8_t reg = 16 + F16(cmd, 4, 4);
    uint8_t val = (F16(cmd, 8, 4) << 4) + F16(cmd, 0, 4);
    sprintf(line[pc].text, "ldi\tr%d,%d\t// $%02x", reg, val, val);
    return true;
}

//----------------------------------------------------------------------
static bool cmd_push_pop()
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC0F) != 0x900F)
        return false;
    uint8_t reg = F16(cmd, 4, 5);
    if( BIT(cmd, 9) )
        sprintf(line[pc].text, "push\tr%d", reg);
    else
        sprintf(line[pc].text, "pop\tr%d", reg);
    return true;
}

//----------------------------------------------------------------------
static bool cmd_reti()
{
    if( code[pc] != 0x9518)
        return false;
    sprintf(line[pc].text, "reti");
    pc--;
    return true;
}

//----------------------------------------------------------------------
static COMMAND_t command[] = {
    cmd_rjmp_rcall,
    cmd_ldi,
    cmd_push_pop,
    cmd_reti
};
#define COMMAND_COUNT (int(sizeof(command)/sizeof(COMMAND_t)))

static bool decode_instruction()
{
    if( line[pc].visited )
        return false;
    int addr = pc;
    for(int i = 0; i < COMMAND_COUNT; i++)
        if( command[i]() )
        {
            line[addr].decoded = true;
            line[addr].visited = true;
            pc++;
            return true;
        }
    return false;
}

//----------------------------------------------------------------------
static bool decode_chain()
{
    pc = origin[0];
    while( !line[pc].decoded )
        if(!decode_instruction())
            return false;
    delete_first_origin();
    return true;
}

//----------------------------------------------------------------------
static bool decode_damp()
{
    while( origin_cnt > 0 )
        if( !decode_chain() )
            return false;
    return true;
}

//----------------------------------------------------------------------
static void print_damp()
{
    for( int i = 0; i < dump_size; i++ )
    {
        printf("%02X", mem_byte[i]);
        if( i % 16 == 15 || i == dump_size - 1) puts("");
    }
}

//----------------------------------------------------------------------
static void print_code()
{
    for( int i = 0; i < MAX_LINES; i++ )
    {
        LINE_t *cline = &line[i];
        if( cline->decoded)
        {
            if( cline->pointed )
                printf("L%X:\t%s\n", i, cline->text);
            else
                printf("\t%s\n", cline->text);
        }
    }
}

//----------------------------------------------------------------------
static bool load_hex(const char *file_name)
{
    FILE *fhex;
    fhex = fopen(file_name, "rt");
    if( fhex != nullptr )
    {
        printf("%s opened\n\n", file_name);
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
        return true;
    }
    else
    {
        printf("Can't open  %s\n", file_name);
        return false;
    }
}

void init_vars()
{
    pc = 0;
    origin[0] = 0;
    for(int i = 0; i < 16; i++)
        origin[i] = i;
    origin_cnt = 16;
}

//----------------------------------------------------------------------
int main()
{
    if (!load_hex("D:\\Proj2019\\Other\\AVR_disasm\\GPig\\GPig.hex") )
        return 0;
//    print_damp();
    init_vars();
    bool result = decode_damp();
    print_code();
    if( result )
        puts("\nDecoding Ok");
    else
        puts("\nDecoding failed");

    return 0;
}
