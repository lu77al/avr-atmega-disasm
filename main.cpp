#include <stdio.h>
#include <string.h>
#include "math_utils.h"

#define FLASH_END   0xFFF
#define FLASH_SIZE  (FLASH_END+1)

#define MEM_SIZE (FLASH_SIZE*2)
#define MAX_LINES FLASH_SIZE
#define MAX_ORIGINS FLASH_SIZE

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

static const char io_name[64][10] =
 {"TWBR", "TWSR", "TWAR", "TWDR", "ADCL", "ADCH", "ADCSRA", "ADMUX", "ACSR", "UBRRL",
  "UCSRB", "UCSRA", "UDR", "SPCR", "SPSR", "SPDR", "PIND",   "DDRD", "PORTD", "PINC",
  "DDRC", "PORTC", "PINB", "DDRB", "PORTB", "$19", "$1A",    "$1B", "EECR", "EEDR",
  "EEARL", "EEARH", "UBRRH", "WDTCR", "ASSR", "OCR2", "TCNT2", "TCCR2", "ICR1L", "ICR1H",
  "OCR1BL", "OCR1BH", "OCR1AL", "OCR1AH", "TCNT1L", "TCNT1H", "TCCR1B", "TCCR1A", "SFIOR", "OSCCAL",
  "TCNT0", "TCCR0", "MCUCSR", "MCUCR", "TWCR", "SPMCR", "TIFR", "TIMSK", "GIFR", "GICR",
  "$3C", "SPL", "SPH", "SREG"};

static const char reg_name[32][4] =
 {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9",
  "r10", "r11", "r12", "r13", "r14", "r15", "r16", "r17", "r18", "r19",
  "r20", "r21", "r22", "r23", "r24", "r25", "XL", "XH", "YL", "YH",
  "ZL", "ZH" };

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
        sprintf(line[pc].text, "movw\t%s:%s, %s:%s",
                reg_name[dst+1], reg_name[dst],
                reg_name[src+1], reg_name[src]);
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
            sprintf(line[pc].text, "cp\t%s,%s", reg_name[dst], reg_name[src]);
        else
            sprintf(line[pc].text, "cpc\t%s,%s", reg_name[dst], reg_name[src]);
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
            sprintf(line[pc].text, "sub\t%s,%s", reg_name[dst], reg_name[src]);
        else
            sprintf(line[pc].text, "sbc\t%s,%s", reg_name[dst], reg_name[src]);
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
                sprintf(line[pc].text, "adc\t%s,%s", reg_name[dst], reg_name[src]);
            else
                sprintf(line[pc].text, "rol\t%s", reg_name[dst]);
        }
        else
        {
            if( dst != src )
                sprintf(line[pc].text, "add\t%s,%s", reg_name[dst], reg_name[src]);
            else
                sprintf(line[pc].text, "lsl\t%s", reg_name[dst]);
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
        sprintf(line[pc].text, "cpse\t%s,%s", reg_name[dst], reg_name[src]);
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
        sprintf(line[pc].text, "and\t%s,%s", reg_name[dst], reg_name[src]);
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
        sprintf(line[pc].text, "eor\t%s,%s", reg_name[dst], reg_name[src]);
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
        sprintf(line[pc].text, "or\t%s,%s", reg_name[dst], reg_name[src]);
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
        sprintf(line[pc].text, "mov\t%s,%s", reg_name[dst], reg_name[src]);
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
        sprintf(line[pc].text, "cpi\t%s,%d\t// $%02x", reg_name[reg], val, val);
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
            sprintf(line[pc].text, "subi\t%s,%d\t// $%02x", reg_name[reg], val, val);
        else
            sprintf(line[pc].text, "sbci\t%s,%d\t// $%02x", reg_name[reg], val, val);
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
        sprintf(line[pc].text, "ori\t%s,%d\t// $%02x", reg_name[reg], val, val);
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
        sprintf(line[pc].text, "andi\t%s,%d\t// $%02x", reg_name[reg], val, val);
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
                sprintf(line[pc].text, "std\tY+$%02x,%s\t// %d", offset, reg_name[reg], offset);
            else
                sprintf(line[pc].text, "std\tZ+$%02x,%s\t// %d", offset, reg_name[reg], offset);
        }
        else
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "ldd\t%s,Y+$%02x\t// %d", reg_name[reg], offset, offset);
            else
                sprintf(line[pc].text, "ldd\t%s,Z+$%02x\t// %d", reg_name[reg], offset, offset);
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
            sprintf(line[pc].text, "sts\t$%04x,%s\t// %d", addr, reg_name[reg], addr);
        else
            sprintf(line[pc].text, "lds\t%s,$%04x\t// %d", reg_name[reg], addr, addr);
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
                sprintf(line[pc].text, "st\tY+,%s", reg_name[reg]);
            else
                sprintf(line[pc].text, "st\tZ+,%s", reg_name[reg]);
        }
        else
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "ld\t%s,Y+", reg_name[reg]);
            else
                sprintf(line[pc].text, "ld\t%s,Z+", reg_name[reg]);
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
                sprintf(line[pc].text, "st\t-Y,%s", reg_name[reg]);
            else
                sprintf(line[pc].text, "st\t-Z,%s", reg_name[reg]);
        }
        else
        {
            if( BIT(cmd, 3) )
                sprintf(line[pc].text, "ld\t%s,-Y", reg_name[reg]);
            else
                sprintf(line[pc].text, "ld\t%s,-Z", reg_name[reg]);
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
            sprintf(line[pc].text, "elpm\t%s,Z", reg_name[reg]);
        else
            sprintf(line[pc].text, "lpm\t%s,Z", reg_name[reg]);
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
            sprintf(line[pc].text, "elpm\t%s,Z+", reg_name[reg]);
        else
            sprintf(line[pc].text, "lpm\t%s,Z+", reg_name[reg]);
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
                sprintf(line[pc].text, "st\tX,%s", reg_name[reg]);
                break;
            case 1:
                sprintf(line[pc].text, "st\tX+,%s", reg_name[reg]);
                break;
            default:
                sprintf(line[pc].text, "st\t-X,%s", reg_name[reg]);
            }
        else
            switch( type ) {
            case 0:
                sprintf(line[pc].text, "ld\t%s,X", reg_name[reg]);
                break;
            case 1:
                sprintf(line[pc].text, "ld\t%s,X+", reg_name[reg]);
                break;
            default:
                sprintf(line[pc].text, "ld\t%s,-X", reg_name[reg]);
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
            sprintf(line[pc].text, "push\t%s", reg_name[reg]);
        else
            sprintf(line[pc].text, "pop\t%s", reg_name[reg]);
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
        sprintf(line[pc].text, "%s\t%s", oo_instr[type], reg_name[reg]);
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
        sprintf(line[pc].text, "dec\t%s", reg_name[reg]);
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
            sprintf(line[pc].text, "call\tL_%X", addr);
        else
            sprintf(line[pc].text, "jmp\tL_%X", addr);
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
static uint8_t cmd_cbi_sbi(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFD00) != 0x9800)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 3, 5);
        uint8_t bit = F16(cmd, 0, 3);
        if( BIT(cmd, 9) )
            sprintf(line[pc].text, "sbi\t%s,%d", io_name[reg], bit);
        else
            sprintf(line[pc].text, "cbi\t%s,%d", io_name[reg], bit);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_sbis_sbic(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFD00) != 0x9900)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 3, 5);
        uint8_t bit = F16(cmd, 0, 3);
        if( BIT(cmd, 9) )
            sprintf(line[pc].text, "sbis\t%s,%d", io_name[reg], bit);
        else
            sprintf(line[pc].text, "sbic\t%s,%d", io_name[reg], bit);
        pc++;
        add_origin(pc + get_instruction_size());
        pc--;
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_mul(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC00) != 0x9C00)
        return 0;
    if( process )
    {
        uint8_t dst = F16(cmd, 4, 5);
        uint8_t src = 16 * F16(cmd, 9, 1) + F16(cmd, 0, 4);
        sprintf(line[pc].text, "mul\t%s,%s", reg_name[dst], reg_name[src]);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_in_out(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xF000) != 0xB000)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        uint8_t io_reg = 16 * F16(cmd, 9, 2) + F16(cmd, 0, 4);
        if( BIT(cmd, 11) )
            sprintf(line[pc].text, "out\t%s,%s", io_name[io_reg], reg_name[reg]);
        else
            sprintf(line[pc].text, "in\t%s,%s", reg_name[reg], io_name[io_reg]);
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
            addr = (pc + 1 - (0x1000 - F16(cmd, 0, 12) )) & FLASH_END;
        else
            addr = (pc + 1 + F16(cmd, 0, 12)) & FLASH_END;
        line[addr].pointed = true;
        if( BIT(cmd, 12) )
        {
            sprintf(line[pc].text, "rcall\tL_%X", addr);
            add_origin(pc+1);
        }
        else
            sprintf(line[pc].text, "rjmp\tL_%X", addr);
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
        sprintf(line[pc].text, "ldi\t%s,%d\t// $%02x", reg_name[reg], val, val);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_cond_branch(bool process)
{
    static const char brbs[8][5] =
        { "brlo", "breq", "brmi", "brvs", "brlt", "brhs", "brts", "brie" };
    static const char brbc[8][5] =
        { "brsh", "brne", "brpl", "brvc", "brge", "brhc", "brtc", "brid" };
    static const char alter_set[8][10] =
        { "\t// brcs", "", "", "", "", "", "", "" };
    static const char alter_clr[8][10] =
        { "\t// brcc", "", "", "", "", "", "", "" };
    uint16_t cmd = code[pc];
    if( (cmd & 0xF800) != 0xF000)
        return 0;
    if( process )
    {
        uint8_t bit = F16(cmd, 0, 3);
        uint8_t offs = F16(cmd, 3, 7);
        uint16_t addr;
        if( BIT(offs, 6) )
            addr = (pc + 1 - (0x80 - offs)) & FLASH_END;
        else
            addr = (pc + 1 + offs) & FLASH_END;
        if( BIT(cmd, 10) )
            sprintf(line[pc].text, "%s\tL_%X%s", brbc[bit], addr, alter_clr[bit]);
        else
            sprintf(line[pc].text, "%s\tL_%X%s", brbs[bit], addr, alter_set[bit]);
        add_origin(addr);
        line[addr].pointed = true;
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_bld_bst(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC08) != 0xF800)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        uint8_t bit = F16(cmd, 0, 3);
        if( BIT(cmd, 9) )
            sprintf(line[pc].text, "bst\t%s,%d", reg_name[reg], bit);
        else
            sprintf(line[pc].text, "bld\t%s,%d", reg_name[reg], bit);
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_sbrs_sbrc(bool process)
{
    uint16_t cmd = code[pc];
    if( (cmd & 0xFC08) != 0xFC00)
        return 0;
    if( process )
    {
        uint8_t reg = F16(cmd, 4, 5);
        uint8_t bit = F16(cmd, 0, 3);
        if( BIT(cmd, 9) )
            sprintf(line[pc].text, "sbrs\t%s,%d", reg_name[reg], bit);
        else
            sprintf(line[pc].text, "sbrc\t%s,%d", reg_name[reg], bit);
        pc++;
        add_origin(pc + get_instruction_size());
        pc--;
    }
    return 1;
}

//----------------------------------------------------------------------
static uint8_t cmd_not_programmed(bool process)
{
    uint16_t cmd = code[pc];
    if( cmd != 0xFFFF)
        return 0;
    if( process )
        pc--;
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
    cmd_cbi_sbi,
    cmd_sbis_sbic,
    cmd_mul,
    cmd_in_out,
    cmd_rjmp_rcall,
    cmd_ldi,
    cmd_cond_branch,
    cmd_bld_bst,
    cmd_sbrs_sbrc,
    cmd_not_programmed
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
static void print_code(const char *file_name)
{
    FILE *fasm;
    fasm = fopen(file_name, "wt");
    fprintf(fasm,".include \"m8def.inc\"\n");
    uint16_t bak_addr = FLASH_END;
    for( int i = 0; i < MAX_LINES; i++ )
    {
        LINE_t *cline = &line[i];
        if( cline->decoded || (!cline->visited && (code[i] != 0xffff)) )
        {
            uint16_t prev = (i - 1) & FLASH_END;
            uint16_t prev_prev = (i - 2) & FLASH_END;
            if(    (bak_addr != prev)
               && ((bak_addr != prev_prev) || !line[prev_prev].visited) )
                fprintf(fasm,".ORG\t$%X\n", i);
            bak_addr = i;
        }
        if( cline->decoded )
        {
            if( cline->pointed )
                fprintf(fasm, "L_%X:\t%s\n", i, cline->text);
            else
                fprintf(fasm, "\t%s\n", cline->text);
        }
        else if( !cline->visited && (code[i] != 0xffff) )
        {
            fprintf(fasm, "L_%X:\t.dw\t$%04x\n", i, code[i]);
        }
    }
    fclose(fasm);
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

#define IRQ_TABLE_SIZE 15

void init_vars()
{
    pc = 0;
    origin[0] = 0;
    for(int i = 0; i < IRQ_TABLE_SIZE; i++)
        origin[i] = i;
    origin_cnt = IRQ_TABLE_SIZE;
}

//#define HEX_FILE "D:\\Proj2019\\Other\\AVR_disasm\\GPig\\GPig.hex"
#define HEX_FILE "D:\\Proj2019\\Other\\AVR_disasm\\MegaDisasm\\heater_dump.hex"
#define ASM_FILE "D:\\Proj2019\\Other\\AVR_disasm\\MegaDisasm\\heater.asm"

//----------------------------------------------------------------------
int main()
{
    if (!load_hex(HEX_FILE) )
        return 0;
    init_vars();
    bool result = decode_dump();
    if( result )
    {
        print_code(ASM_FILE);
        puts("\nDecoding Ok");
    }
    else
    {
        puts("\nDecoding failed\n");
        print_dump();
    }

    return 0;
}
