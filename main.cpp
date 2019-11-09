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

typedef uint8_t (*COMMAND_t)(bool process);

static uint8_t get_instruction_size();

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
static uint8_t cmd_nop(bool process)
{
    if( code[pc] != 0x0000)
        return 0;
    if( process )
        sprintf(line[pc].text, "nop");
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_movw(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFF00) != 0x0100)
        return 0;
    if( process )
    {
        uint8_t dst = 2 * F16(cmd, 4, 4);
        uint8_t src = 2 * F16(cmd, 0, 4);
        sprintf(line[pc].text, "movw\tr%d:r%d, r%d:r%d", dst+1, dst, src+1, src);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_cpc_cp(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xEC00) != 0x0400)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        if( BIT(cmd, 12) )
            sprintf(line[pc].text, "cp\tr%d,r%d", dst, src);
        else
            sprintf(line[pc].text, "cpc\tr%d,r%d", dst, src);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_sub_sbc(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xEC00) != 0x0800)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        if( BIT(cmd, 12) )
            sprintf(line[pc].text, "sub\tr%d,r%d", dst, src);
        else
            sprintf(line[pc].text, "sbc\tr%d,r%d", dst, src);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_add_adc_lsl_rol(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xEC00) != 0x0C00)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        if( BIT(cmd, 12) )
        {
            if( dst != src )
                sprintf(line[pc].text, "adc\tr%d,r%d", dst, src);
            else
                sprintf(line[pc].text, "rol\tr%d", dst);
        }
        else
        {
            if( dst != src )
                sprintf(line[pc].text, "add\tr%d,r%d", dst, src);
            else
                sprintf(line[pc].text, "lsl\tr%d", dst);
        }
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_cpse(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC00) != 0x1000)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "cpse\tr%d,r%d", dst, src);
        pc++;
        add_origin(pc + get_instruction_size());
        pc--;
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_and(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC00) != 0x2000)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "and\tr%d,r%d", dst, src);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_eor(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC00) != 0x2400)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "eor\tr%d,r%d", dst, src);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_or(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC00) != 0x2800)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "or\tr%d,r%d", dst, src);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_mov(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC00) != 0x2C00)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "mov\tr%d,r%d", dst, src);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_cpi(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xF000) != 0x3000)
        return 0;
    if( process )
    {
        uint8_t reg = 16 + F16(cmd, 4, 4);
        uint8_t val = (F16(cmd, 8, 4) << 4) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "cpi\tr%d,%d\t// $%02x", reg, val, val);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_subi_sbci(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xE000) != 0x4000)
        return 0;
    if( process )
    {
        uint8_t reg = 16 + F16(cmd, 4, 4);
        uint8_t val = (F16(cmd, 8, 4) << 4) + F16(cmd, 0, 4);
        if( BIT(cmd, 12) )
            sprintf(line[pc].text, "subi\tr%d,%d\t// $%02x", reg, val, val);
        else
            sprintf(line[pc].text, "sbci\tr%d,%d\t// $%02x", reg, val, val);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_ori(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xF000) != 0x6000)
        return 0;
    if( process )
    {
        uint8_t reg = 16 + F16(cmd, 4, 4);
        uint8_t val = (F16(cmd, 8, 4) << 4) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "ori\tr%d,%d\t// $%02x", reg, val, val);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_andi(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xF000) != 0x7000)
        return 0;
    if( process )
    {
        uint8_t reg = 16 + F16(cmd, 4, 4);
        uint8_t val = (F16(cmd, 8, 4) << 4) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "andi\tr%d,%d\t// $%02x", reg, val, val);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_ldd_std(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xD000) != 0x8000)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        uint8_t offset = (F16(cmd, 13, 1) << 5) + (F16(cmd, 10, 2) << 3) + F16(cmd, 0, 3);
        if( BIT(cmd, 9) )
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "std\tY+$%02x,r%d\t// %d", offset, reg, offset);
            else
                sprintf(line[pc].text, "std\tZ+$%02x,r%d\t// %d", offset, reg, offset);
        }
        else
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "ldd\tr%d,Y+$%02x\t// %d", reg, offset, offset);
            else
                sprintf(line[pc].text, "ldd\tr%d,Z+$%02x\t// %d", reg, offset, offset);
        }
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_lds_sts(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC0F) != 0x9000)
        return 0;
    if( process )
    {
        uint8_t  reg = F16(cmd, 4, 5);
        uint16_t addr = code[pc+1];
        line[pc+1].visited = true;
        if( BIT(cmd, 9) )
            sprintf(line[pc].text, "sts\t$%04x,r%d\t// %d", addr, reg, addr);
        else
            sprintf(line[pc].text, "lds\tr%d,$%04x\t// %d", reg, addr, addr);
    }
    return 2;
}

//----------------------------------------------------------------------
static uint8_t cmd_ld_st_plus(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC07) != 0x9001)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        if( BIT(cmd, 9) )
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "st\tY+,r%d", reg);
            else
                sprintf(line[pc].text, "st\tZ+,r%d", reg);
        }
        else
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "ld\tr%d,Y+", reg);
            else
                sprintf(line[pc].text, "ld\tr%d,Z+", reg);
        }
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_ld_st_minus(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC07) != 0x9002)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        if( BIT(cmd, 9) )
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "st\t-Y,r%d", reg);
            else
                sprintf(line[pc].text, "st\t-Z,r%d", reg);
        }
        else
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "ld\tr%d,-Y", reg);
            else
                sprintf(line[pc].text, "ld\tr%d,-Z", reg);
        }
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_e_lpm(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFE0D) != 0x9004)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        if( BIT(cmd, 1) )
            sprintf(line[pc].text, "elpm\tr%d,Z", reg);
        else
            sprintf(line[pc].text, "lpm\tr%d,Z", reg);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_e_lpm_plus(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFE0D) != 0x9005)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        if( BIT(cmd, 1) )
            sprintf(line[pc].text, "elpm\tr%d,Z+", reg);
        else
            sprintf(line[pc].text, "lpm\tr%d,Z+", reg);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_ld_st_x(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC0C) != 0x900C)
        return 0;
    uint8_t type = F16(cmd, 0, 2);
    if( type == 3 )
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        if( BIT(cmd, 9) )
            switch( type ) {
            case 0:
                sprintf(line[pc].text, "st\tX,r%d", reg);
                break;
            case 1:
                sprintf(line[pc].text, "st\tX+,r%d", reg);
                break;
            default:
                sprintf(line[pc].text, "st\t-X,r%d", reg);
            }
        else
            switch( type ) {
            case 0:
                sprintf(line[pc].text, "ld\tr%d,X", reg);
                break;
            case 1:
                sprintf(line[pc].text, "ld\tr%d,X+", reg);
                break;
            default:
                sprintf(line[pc].text, "ld\tr%d,-X", reg);
            }
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_push_pop(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC0F) != 0x900F)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        if( BIT(cmd, 9) )
            sprintf(line[pc].text, "push\tr%d", reg);
        else
            sprintf(line[pc].text, "pop\tr%d", reg);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_one_operand(bool process)
{
    static const char oo_instr[8][5] =
        { "com", "neg", "swap", "inc", "", "asr", "lsr", "ror" };
    uint16_t cmd = code[pc];
    if( (cmd & 0xFE08) != 0x9400)
        return 0;
    uint8_t type = F16(cmd, 0, 3);
    if( type == 4 )
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        sprintf(line[pc].text, "%s\tr%d", oo_instr[type], reg);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_sex_clx(bool process)
{
    static const char *status_bit = "cznvshti";
    uint16_t cmd = code[pc];
    if( (cmd & 0xFF0F) != 0x9408)
        return 0;
    if( process )
    {
        uint8_t bit = F16(cmd, 4, 3);
        if( BIT(cmd, 7) )
            sprintf(line[pc].text, "cl%c", status_bit[bit]);
        else
            sprintf(line[pc].text, "se%c", status_bit[bit]);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_ret_reti(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFFEF) != 0x9508 )
        return 0;
    if( process )
    {
        if( BIT(cmd, 4) )
            sprintf(line[pc].text, "reti");
        else
            sprintf(line[pc].text, "ret");
        pc--;
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_misc(bool process)
{
    static const char instr[7][8] =
        { "sleep", "break", "wdr", "lpm", "elpm", "spm", "spm Z+" };
    static const uint8_t instr_code[7] =
        {   0x8,     0x9,     0xA,  0xC,    0xD,   0xE,    0xF    };
    uint16_t cmd = code[pc];
    if( (cmd & 0xFF0F) != 0x9508)
        return 0;
    uint8_t type = F16(cmd, 4, 4);
    uint8_t i = 0;
    for(; (i < 7) && (instr_code[i] != type); i++ );
    if( i >= 7 )
        return 0;
    if( process )
        sprintf(line[pc].text, instr[i]);
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_ijmp_icall(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFEEF) != 0x9409)
        return 0;
    if( process )
    {
        if( BIT(cmd, 8) )
            sprintf(line[pc].text, "icall");
        else
            sprintf(line[pc].text, "ijmp");
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_dec(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFE0F) != 0x940A)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        sprintf(line[pc].text, "dec\tr%d", reg);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_jmp_call(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFE0C) != 0x940C)
        return 0;
    if( process )
    {
        uint16_t addr = code[pc+1];
        line[addr].pointed = true;
        line[pc+1].visited = true;
        if( BIT(cmd, 1) )
            sprintf(line[pc].text, "call\tL%X", addr);
        else
            sprintf(line[pc].text, "jmp\tL%X", addr);
        pc = addr - 2;
    }
    return 2;
}

//----------------------------------------------------------------------
static uint8_t cmd_adiw_subiw(bool process)
{
    static const char reg_apir[4][16] =
        { "W", "XH:XL", "YH:YL", "ZH:ZL" };
    uint16_t cmd = code[pc];
    if( (cmd & 0xFE00) != 0x9600)
        return 0;
    if( process )
    {
        uint16_t pair = F16(cmd, 4, 2);
        uint8_t val = (F16(cmd, 6, 2) << 4) + F16(cmd, 0, 4);
        if( BIT(cmd, 8) )
            sprintf(line[pc].text, "sbiw\t%s,%d\t// %02X", reg_apir[pair], val, val);
        else
            sprintf(line[pc].text, "adiw\t%s,%d\t// %02X", reg_apir[pair], val, val);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_rjmp_rcall(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xE000) != 0xC000)
        return 0;
    if( process )
    {
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
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_ldi(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xF000) != 0xE000)
        return 0;
    if( process )
    {
        uint8_t reg = 16 + F16(cmd, 4, 4);
        uint8_t val = (F16(cmd, 8, 4) << 4) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "ldi\tr%d,%d\t// $%02x", reg, val, val);
    }
    return 1;
}


//----------------------------------------------------------------------
static COMMAND_t command[] = {
    cmd_nop,
    cmd_movw,
    cmd_cpc_cp,
    cmd_sub_sbc,
    cmd_add_adc_lsl_rol,
    cmd_cpse,
    cmd_and,
    cmd_eor,
    cmd_or,
    cmd_mov,
    cmd_cpi,
    cmd_subi_sbci,
    cmd_ori,
    cmd_andi,
    cmd_ldd_std,
    cmd_lds_sts,
    cmd_ld_st_plus,
    cmd_ld_st_minus,
    cmd_e_lpm,
    cmd_e_lpm_plus,
    cmd_ld_st_x,
    cmd_push_pop,
    cmd_one_operand,
    cmd_sex_clx,
    cmd_ret_reti,
    cmd_misc,
    cmd_ijmp_icall,
    cmd_dec,
    cmd_jmp_call,
    cmd_adiw_subiw,

    cmd_rjmp_rcall,
    cmd_ldi
};
#define COMMAND_COUNT (int(sizeof(command)/sizeof(COMMAND_t)))

//----------------------------------------------------------------------
static bool decode_instruction()
{
    if( line[pc].visited )
        return false;
    int addr = pc;
    for(int i = 0; i < COMMAND_COUNT; i++)
    {
        uint8_t size = command[i](true);
        if( size != 0 )
        {
            line[addr].decoded = true;
            line[addr].visited = true;
            pc += size;
            return true;
        }
    }
    return false;
}

//----------------------------------------------------------------------
static uint8_t get_instruction_size()
{
    for(int i = 0; i < COMMAND_COUNT; i++)
    {
        uint8_t size = command[i](false);
        if( size != 0 )
            return size;
    }
    return 0;
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
static bool decode_dump()
{
    while( origin_cnt > 0 )
        if( !decode_chain() )
            return false;
    return true;
}

//----------------------------------------------------------------------
static void print_dump()
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
    init_vars();
    bool result = decode_dump();
    print_code();
    if( result )
        puts("\nDecoding Ok");
    else
    {
        puts("\nDecoding failed\n");
        print_dump();
    }

    return 0;
}
