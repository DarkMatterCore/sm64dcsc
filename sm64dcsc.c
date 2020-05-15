#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#define VERSION             "0.1"

#define SAVE_FILE_COUNT     4
#define SAVE_FILE_MAGIC     (uint16_t)0x4441    /* "DA" */

#define GLOBAL_DATA_MAGIC   (uint16_t)0x4849    /* "HI" */

#define MAX_COURSE_COUNT    25                  /* Not considering Castle course */
#define COURSE_STAGE_COUNT  15

#define MAX_ELEMENTS(x)     ((sizeof((x))) / (sizeof((x)[0])))

#define OUT_SAVE_NAME       "sm64_save_file"
#define N64_SAVE_SUFFIX     "_n64"
#define DECOMP_SAVE_SUFFIX  "_decomp"
#define OUT_SAVE_EXT        ".bin"

#define FULL_SAVE_NAME(x)   ((x) == SaveType_N64 ? (OUT_SAVE_NAME N64_SAVE_SUFFIX OUT_SAVE_EXT) : (OUT_SAVE_NAME DECOMP_SAVE_SUFFIX OUT_SAVE_EXT))

/* Type definitions. */

typedef struct {
    uint16_t magic;
    uint16_t checksum;
} SaveBlockSignature;

typedef struct {
    uint8_t cap_level;
    uint8_t cap_area;
    int16_t x_coord;
    int16_t y_coord;
    int16_t z_coord;
} LostMarioCapInfo;

typedef struct {
    uint8_t star1           : 1;
    uint8_t star2           : 1;
    uint8_t star3           : 1;
    uint8_t star4           : 1;
    uint8_t star5           : 1;
    uint8_t star6           : 1;
    uint8_t star7           : 1;
    uint8_t prev_lvl_cannon : 1;
} CourseStarInfo;

typedef struct {
    uint8_t file_exists             : 1;
    uint8_t has_wing_cap            : 1;
    uint8_t has_metal_cap           : 1;
    uint8_t has_vanish_cap          : 1;
    uint8_t has_basement_door_key   : 1;
    uint8_t has_upstairs_door_key   : 1;
    uint8_t unlocked_basement_door  : 1;
    uint8_t unlocked_upstairs_door  : 1;
    uint8_t ddd_moved_back          : 1;
    uint8_t castle_moat_drained     : 1;
    uint8_t unlocked_pss_door       : 1;
    uint8_t unlocked_wf_door        : 1;
    uint8_t unlocked_ccm_door       : 1;
    uint8_t unlocked_jrb_door       : 1;
    uint8_t unlocked_bitdw_door     : 1;
    uint8_t unlocked_bitfs_door     : 1;
    uint8_t cap_lost_on_ground      : 1;
    uint8_t cap_lost_on_ssl         : 1;
    uint8_t cap_lost_on_ttm         : 1;
    uint8_t cap_lost_on_sl          : 1;
    uint8_t unlocked_3rd_floor_door : 1;
    uint8_t unused_1                : 1;
    uint8_t unused_2                : 1;
    uint8_t unused_3                : 1;
    CourseStarInfo castle_stars;
} ProgressFlags;

typedef struct {
    LostMarioCapInfo lost_cap_info;
    ProgressFlags flags;
    CourseStarInfo course_stars[MAX_COURSE_COUNT];
    uint8_t course_coin_scores[COURSE_STAGE_COUNT];
    SaveBlockSignature signature;
} SaveFileBlock;

typedef enum {
    SoundMode_Stereo  = 0,
    SoundMode_Mono    = 1,
    SoundMode_Headset = 2
} SoundMode;

typedef enum {
    Language_English = 0,
    Language_French  = 1,
    Language_German  = 2
} Language;

typedef struct {
    uint32_t coin_scores_ages[SAVE_FILE_COUNT];
    uint16_t sound_mode;                        ///< SoundMode.
    uint16_t language;                          ///< Language.
    uint8_t padding[0x08];
    SaveBlockSignature signature;
} GlobalDataBlock;

typedef struct {
    SaveFileBlock files[SAVE_FILE_COUNT][2];    ///< Each save file has its own backup section.
    GlobalDataBlock global_data[2];             ///< Global data has its own backup setion.
} SaveBuffer;

typedef enum {
    SaveType_Invalid = 0,
    SaveType_N64     = 1,
    SaveType_Decomp  = 2
} SaveType;

typedef struct {
    char name[0x10];
    uint8_t save_type;  ///< SaveType.
} DestFormatType;

/* Global variables. */

static SaveBuffer save_buf = {0};

static bool big_endian_flag = false;

static DestFormatType dest_formats[] = {
    { "n64", SaveType_N64 },
    { "decomp", SaveType_Decomp },
};

static uint32_t dest_format_cnt = MAX_ELEMENTS(dest_formats);

/* Function prototypes. */

static bool toolsIsBigEndian(void);
static bool toolsValidateDestFormat(char *dest_format_str, uint8_t *dest_type);
static void toolsPrintUsage(char **argv);
static uint8_t toolsGetSaveType(void);
static void toolsConvertSave(uint8_t save_type, uint8_t dest_type);

int main(int argc, char **argv)
{
    int ret = 0;
    
    FILE *fd = NULL;
    size_t save_size = 0, fres = 0;
    
    uint8_t save_type = SaveType_Invalid, dest_type = SaveType_Invalid;
    
    char outpath[0x1000] = {0};
    bool remove_outfile = false;
    
    big_endian_flag = toolsIsBigEndian();
    
    printf("\n\tSuper Mario 64 Decompilation Save Converter v%s - By DarkMatterCore\n\n", VERSION);
    
    printf("\tDetected CPU endianness: %s Endian.\n", (big_endian_flag ? "Big" : "Little"));
    
    if (argc != 3 || !toolsValidateDestFormat(argv[2], &dest_type))
    {
        ret = -1;
        toolsPrintUsage(argv);
        goto out;
    }
    
    fd = fopen(argv[1], "rb");
    if (!fd)
    {
        ret = -2;
        printf("\n\tError opening \"%s\" for reading.\n", argv[1]);
        goto out;
    }
    
    fseek(fd, 0, SEEK_END);
    save_size = ftell(fd);
    rewind(fd);
    
    if (save_size < sizeof(SaveBuffer))
    {
        ret = -3;
        printf("\n\tInvalid input save size!\n");
        goto out;
    }
    
    fres = fread(&save_buf, 1, sizeof(SaveBuffer), fd);
    if (fres != sizeof(SaveBuffer))
    {
        ret = -4;
        printf("\n\tFailed to read save data!\n");
        goto out;
    }
    
    fclose(fd);
    fd = NULL;
    
    /* Get save type */
    save_type = toolsGetSaveType();
    if (save_type == SaveType_Invalid)
    {
        ret = -5;
        printf("\n\tInvalid input save!\n");
        goto out;
    }
    
    printf("\tDetected save type: %s.\n\n", save_type == SaveType_N64 ? "N64 (Big Endian)" : "Decomp (Little Endian)");
    
    if (save_type == dest_type)
    {
        printf("\tThere's no need to convert this savefile.\n\tYou can use it directly with a SM64 port for this CPU architecture.\n");
        goto out;
    }
    
    /* Convert save */
    toolsConvertSave(save_type, dest_type);
    
    /* Save output file */
    snprintf(outpath, MAX_ELEMENTS(outpath), argv[1]);
    
    char *tmp = strrchr(outpath, '\\');
    if (!tmp) tmp = strrchr(outpath, '/');
    
    if (tmp)
    {
        tmp++;
        *tmp = '\0';
    } else {
        *outpath = '\0';
    }
    
    strcat(outpath, FULL_SAVE_NAME(dest_type));
    
    fd = fopen(outpath, "wb");
    if (!fd)
    {
        ret = -6;
        printf("\n\tError opening \"%s\" for writing.\n", outpath);
        goto out;
    }
    
    fres = fwrite(&save_buf, 1, sizeof(SaveBuffer), fd);
    if (fres != sizeof(SaveBuffer))
    {
        ret = -7;
        remove_outfile = true;
        printf("\n\tFailed to write converted save data!\n");
        goto out;
    }
    
    printf("\tProcess succesfully finished! Output file: \"%s\".\n\n", outpath);
    
out:
    if (fd) fclose(fd);
    if (remove_outfile) remove(outpath);
    
    return ret;
}

static bool toolsIsBigEndian(void)
{
    union {
        uint32_t i;
        uint8_t c[4];
    } test_var = { 0x01020304 };
    
    return (test_var.c[0] == 0x01);
}

static bool toolsValidateDestFormat(char *dest_format_str, uint8_t *dest_type)
{
    if (!dest_format_str || !strlen(dest_format_str) || !dest_type) return false;
    
    for(uint32_t i = 0; i < dest_format_cnt; i++)
    {
        if (!strcmp(dest_format_str, dest_formats[i].name))
        {
            *dest_type = dest_formats[i].save_type;
            return true;
        }
    }
    
    return false;
}

static void toolsPrintUsage(char **argv)
{
    if (!argv || !argv[0] || !strlen(argv[0])) return;
    
    printf("\n\tUsage: %s [infile] [dst_fmt]\n\n", argv[0]);
    printf("\t\t- infile: Name of the input save file.\n");
    printf("\t\t- dst_fmt: Output save file format.\n\n");
    printf("\tPossible output format values:\n\n");
    printf("\t\t- \"n64\": N64 save format (Big Endian, works with SM64 ports for BE CPUs).\n");
    printf("\t\t- \"decomp\": Common decompilation save format (Little Endian).\n\n");
    printf("\tNotes:\n\n");
    printf("\t\t- Input format is automatically detected (N64 vs Decomp).\n");
    printf("\t\t- Output format will always match the CPU endianness.\n");
    printf("\t\t- RetroArch SRM saves are supported.\n");
    printf("\t\t- Output file will get saved to the same directory as the input file.\n");
    printf("\t\t- Output file name will either be \"%s\" or \"%s\" depending\n\t\t  on the output type.\n", FULL_SAVE_NAME(SaveType_N64), FULL_SAVE_NAME(SaveType_Decomp));
}

static uint8_t toolsGetSaveType(void)
{
    uint8_t cur_type = SaveType_Invalid, prev_type = SaveType_Invalid;
    
    /* Check save files */
    for(uint8_t i = 0; i < SAVE_FILE_COUNT; i++)
    {
        /* Validate magic words from current save file */
        if ((save_buf.files[i][0].signature.magic != SAVE_FILE_MAGIC && save_buf.files[i][0].signature.magic != __builtin_bswap16(SAVE_FILE_MAGIC)) || \
            (save_buf.files[i][1].signature.magic != SAVE_FILE_MAGIC && save_buf.files[i][1].signature.magic != __builtin_bswap16(SAVE_FILE_MAGIC)) || \
            save_buf.files[i][0].signature.magic != save_buf.files[i][1].signature.magic) return SaveType_Invalid;
        
        /* Check magic word endianness */
        uint16_t magic = save_buf.files[i][0].signature.magic;
        
        if (big_endian_flag)
        {
            cur_type = (magic == SAVE_FILE_MAGIC ? SaveType_N64 : SaveType_Decomp);
        } else {
            cur_type = (magic == SAVE_FILE_MAGIC ? SaveType_Decomp : SaveType_N64);
        }
        
        if (i > 0 && cur_type != prev_type) return SaveType_Invalid;
        
        prev_type = cur_type;
    }
    
    /* Check global data */
    /* Validate magic words */
    if ((save_buf.global_data[0].signature.magic != GLOBAL_DATA_MAGIC && save_buf.global_data[0].signature.magic != __builtin_bswap16(GLOBAL_DATA_MAGIC)) || \
        (save_buf.global_data[1].signature.magic != GLOBAL_DATA_MAGIC && save_buf.global_data[1].signature.magic != __builtin_bswap16(GLOBAL_DATA_MAGIC)) || \
        save_buf.global_data[0].signature.magic != save_buf.global_data[1].signature.magic) return SaveType_Invalid;
    
    /* Check magic word endianness */
    uint16_t magic = save_buf.global_data[0].signature.magic;
    
    if (big_endian_flag)
    {
        cur_type = (magic == SAVE_FILE_MAGIC ? SaveType_N64 : SaveType_Decomp);
    } else {
        cur_type = (magic == SAVE_FILE_MAGIC ? SaveType_Decomp : SaveType_N64);
    }
    
    if (cur_type != prev_type) return SaveType_Invalid;
    
    return cur_type;
}

static void toolsConvertSave(uint8_t save_type, uint8_t dest_type)
{
    if (save_type == dest_type) return;
    
    uint32_t flags = 0;
    
    /* Process save files */
    for(uint8_t i = 0; i < SAVE_FILE_COUNT; i++)
    {
        for(uint8_t j = 0; j < 2; j++)
        {
            /* Byteswap lost cap coordinates */
            save_buf.files[i][j].lost_cap_info.x_coord = __builtin_bswap16(save_buf.files[i][j].lost_cap_info.x_coord);
            save_buf.files[i][j].lost_cap_info.y_coord = __builtin_bswap16(save_buf.files[i][j].lost_cap_info.y_coord);
            save_buf.files[i][j].lost_cap_info.z_coord = __builtin_bswap16(save_buf.files[i][j].lost_cap_info.z_coord);
            
            /* Byteswap progress flags */
            memcpy(&flags, &(save_buf.files[i][j].flags), sizeof(uint32_t));
            flags = __builtin_bswap32(flags);
            memcpy(&(save_buf.files[i][j].flags), &flags, sizeof(uint32_t));
            
            /* Byteswap signature */
            save_buf.files[i][j].signature.magic = __builtin_bswap16(save_buf.files[i][j].signature.magic);
            save_buf.files[i][j].signature.checksum = __builtin_bswap16(save_buf.files[i][j].signature.checksum);
        }
    }
    
    /* Process global data */
    for(uint8_t i = 0; i < 2; i++)
    {
        /* Byteswap coin scores ages */
        for(uint8_t j = 0; j < SAVE_FILE_COUNT; j++) save_buf.global_data[i].coin_scores_ages[j] = __builtin_bswap32(save_buf.global_data[i].coin_scores_ages[j]);
        
        /* Byteswap sound mode */
        save_buf.global_data[i].sound_mode = __builtin_bswap16(save_buf.global_data[i].sound_mode);
        
        /* Byteswap language */
        save_buf.global_data[i].language = __builtin_bswap16(save_buf.global_data[i].language);
        
        /* Byteswap signature */
        save_buf.global_data[i].signature.magic = __builtin_bswap16(save_buf.global_data[i].signature.magic);
        save_buf.global_data[i].signature.checksum = __builtin_bswap16(save_buf.global_data[i].signature.checksum);
    }
}
