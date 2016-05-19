/*

This is a work in progress, a CLI utility to probe and write to a FAT16/32 filesystem on a flash or usb drive.
This is mostly a proof of concept to reinforce my understanding of both filesystems, but it might make its way into another project some day.

I based this on a tutorial written by Joonas Pihlajamaa, accessed at: http://codeandlife.com/2012/04/02/simple-fat-and-sd-tutorial-part-1/
I also learned a lot from the Microsoft specification, and this oveview of flash filesystems: http://www.compuphase.com/mbr_fat.htm

*/
#include <stdio.h>
#include <stdlib.h>

typedef struct{
    /*where this partition starts */
    unsigned char first_byte;
    
    /*CHS Addressing Start */
    unsigned char CHSStart[3];
    
    /*partition type identifier */
    unsigned char partitionType;
    
    /*CHS Addressing end */
    unsigned char CHSEnd[3];

    /* First sector of partition linear offset in sectors */
    unsigned long startSector;
    
  
    unsigned long lengthInSectors;
} __attribute((packed)) PartitionTable;

/***NOTE*** Needs to be packed to prevent compiler from mucking
around with the data fields, we want to directly fill our structs
and any sort of padding will disrupt that.  Alternately, we could read
them in one at a time using fgetc() or fscanf()  */


typedef struct{
    /*for CHS, determines where it begins */
    unsigned char jumpCode [3];
    
    /* For compatibility with old drivers, not used here, but we will grab
    everything at once to simplify the process of actually reading the file */
    char oem [8];
    
    /* Size of each sector, usually 512 bytes*/
    unsigned short sectorSize;
    
    /* Sectors per cluster */
    unsigned char secPerClus;

    /* Count of reserved sectors */
    unsigned short rsvdSecCnt;

    /* Number of FATs present*/
    unsigned char numFats;

    /* FAT16 only, number of 32 byte directory entries present in the root*/
    unsigned short rootEntCnt;

    /* Total count of sectors, FAT16 only.  This entry is mutually exclusive
    with totalSec32, and will be used to determine the system type */
    unsigned short totalSec16;

    /* This is not used anymore, but can designate whether the media is a
    removeable disk.  Valids are 0xF(0, 8-F)+ */
    unsigned char mediaDesc;
    
    /* This is the size in sectors of the FAT16*/
    unsigned short FAT16Sz;
    
    /* This is relevant for CHS systems where geometry is important*/
    unsigned short secPerTrack;
    
    /* Number of heads for CHS systems where geometry is important*/
    unsigned short numHeads;
    
    /* This is for specific OS's in specific partition configurations.  In other
    words, if you don't know what it's for, you don't need it. */
    unsigned long illuminati;
    
    /* Total count of sectors, FAT32 only.  Again, mutuallu exclusive with the 
    matching field for FAT16, and it will be used to determine which scheme is
    in place. */
    unsigned long totalSec32;
    
    /***NOTE*** The remaining fields are only present in FAT16 systems */

    /* This is a drive identification number for use with an MSDOS bootstrap
    utility.  OS-specific and probably not relevant for you. */
    unsigned char driveNum;

    /* This is for Windows NT only, and is always 0.*/
    unsigned char itsTradition;

    /* This field is here to signal that the next three fields are populated.
    These fields are part of an extended boot signature which will uniquely 
    identify a drive */
    unsigned char bootSig;
    
    /* Serial number*/
    unsigned long volumeID;

    /* Label*/
    char volumeLabel [11];

    /* File system type - note that this is generally an unreliable way to 
    identify the type, since it may not be set correctly if at all. */
    char fileSysType [8];
    
    /*In case we need it, we have it, and we don't have to bother with figuring
    out what is left to read from boot sector*/
    char bootCode [448];
    unsigned short bootSectorSig;
    
} __attribute((packed)) FAT16BootSector;


typedef struct{
    /*for CHS, determines where it begins */
    unsigned char jumpCode [3];
    
    /* For compatibility with old drivers, not used here, but we will grab
    everything at once to simplify the process of actually reading the file */
    char oem [8];
    
    /* Size of each sector, usually 512 bytes*/
    unsigned short sectorSize;
    
    /* Sectors per cluster */
    unsigned char secPerClus;

    /* Count of reserved sectors */
    unsigned short rsvdSecCnt;

    /* Number of FATs present*/
    unsigned char numFats;

    /* FAT16 only, number of 32 byte directory entries present in the root*/
    unsigned short rootEntCnt;

    /* Total count of sectors, FAT16 only.  This entry is mutually exclusive
    with totalSec32, and will be used to determine the system type */
    unsigned short totalSec16;

    /* This is not used anymore, but can designate whether the media is a
    removeable disk.  Valids are 0xF(0, 8-F)+ */
    unsigned char mediaDesc;
    
    /* This is the size in sectors of the FAT16*/
    unsigned short FAT16Sz;
    
    /* This is relevant for CHS systems where geometry is important*/
    unsigned short secPerTrack;
    
    /* Number of heads for CHS systems where geometry is important*/
    unsigned short numHeads;
    
    /* This is for specific OS's in specific partition configurations.  In other
    words, if you don't know what it's for, you don't need it. */
    unsigned long illuminati;
    
    /* Total count of sectors, FAT32 only.  Again, mutuallu exclusive with the 
    matching field for FAT16, and it will be used to determine which scheme is
    in place. */
    unsigned long totalSec32;

    /***NOTE*** The remaining fields are only present in FAT32 systems */

    /* The size of each FAT in 32-bit sectors*/
    unsigned short FAT32Sz;

    /* Flags for extended FAT systems, see the specs for more info*/
    unsigned char extFlags [2];

    /* This is a version number to ensure compatibility with older FAT32 
    systems.  According to the specs, drives should not be mounted if the
    version is not recognized.  Of course, we will ignore this warning.*/
    char fileSysVersion [2];

    /* This is the cluster number offset from the beginning of the boot sector
    to the root directories */
    unsigned long rootClus;
    
    /*In case we need it, we have it, and we don't have to bother with figuring
    out what is left to read from boot sector*/
    char otherJunk [42];

} __attribute((packed)) FAT32BootSector;

typedef struct{
    /* File name, cannot start with 0x20, or special codes:
    
    0x00 - This file has never been used, don't bother checkiong the rest
    0xE5 - This file has been "deleted", no need to read the rest
    0x05 - This filename actually begins with 0xE5
    0x2E - This is a directory, not a file.
            For directories in the root, the second byte will be 0x0000
            For directories elsewhere, if the second byte is also 0x2E, then
            the startCluster field will point to the parent directory
    
    */
    unsigned char filename [8];
    
    /* file extension, dot implied */
    unsigned char fileExt [3];
    
    /*
    0x01    read only
    0x02    hidden
    0x04    system & hidden
    0x08    This is a volume label (root entry only)
    0x10    This is a subdirectory
    0x20    Archiving flag, set when madified and unset by backup utilities
    0x40    Always zero, bad touch
    0x80    Always zero, bad touch
    */
    unsigned char attrFlags;
    
    /*Bad touch*/
    unsigned char reserved [10];
    
    /* Time created/last modified
        hhhhhmmmmmmxxxxx -> binary hours, minutes, seconds (in increments of 2)
    */
    unsigned short time;
    
    /* Date created/last modifief
        yyyyyyymmmmddddd -> binary years (since 1980), month, day
    */
    unsigned short date;
    
    /* Starting cluster, note this is always at least 0x0002 because of the FAT        
        and boot sector    
    */
    unsigned short startCluster;
    unsigned long fileSize;

} __attribute((packed)) FATEntry;

/* Prints out all the partitions gleaned from sweeping the MBR */
void printPartitions(PartitionTable* pt);

/* Prints out all fields of the boot sector, FAT32 scheme */
void print32(FAT32BootSector* bs);

/* Prints out all fields of the boot sector, FAT16 scheme */
void print16(FAT16BootSector* bs);

/* Blindly compares two strings up to the 500th character or a null character.
Returns 1 if they are identical, 0 otherwise. */
int compareStr(char* str1, char* str2);

/*Prints the file metadata as read from the root directory */
void printDir(FATEntry* entry);


int main(int argc, char* argv[]){

    FILE * infile = fopen(argv[1], "rb");
    int i, noDebug, noMBR, found32, found16;
    int FirstDataSec, FirstSecOfCluster;
    PartitionTable partitions[4];
    FAT16BootSector bs16;
    FAT32BootSector bs32;
    noDebug = noMBR = found16 = 0;
   
    if(argc < 2){
        printf("Usage: ./MBRBoot <file> <arg>?\n");
        printf("where <file> is the mounted filesystem directory\n");
        printf("where <arg> is nothing, or exactly one of the following flags:\n");
        printf("\'--q\'\t\tQuiet mode, no metadata\n");
        printf("\'--noMBR\'\t\tDo not try to parse a MBR\n");
        return -1;
    }
    if(argc == 3){
        noDebug = compareStr("--q", argv[2]);
        noMBR = compareStr("--noMBR", argv[2]);
    }
    if(argc == 4){
        noDebug = compareStr("--q", argv[2]) ? 1: (compareStr("--q", argv[3]) ? 1:0);
        noMBR = compareStr("--noMBR", argv[2]) ? 1: (compareStr("--noMBR", argv[3]) ? 1:0);
    }
    printf("Flags: [quiet mode: %d] [no MBR: %d]\n\n", noDebug, noMBR); 
    if(noMBR){
        //check the offset for FAT16Sz to determine if this is FAT16 or 32
        fseek(infile, 22, SEEK_SET);
        fread(&found16, 2, 1, infile);
        printf("Value of found16: %d, %ld, %2X\n\n", found16, found16, found16);        
        
        //Boot should begin at the first sector of the reserved section
        fseek(infile, 0, SEEK_SET);

        
        if(found16){
            fread(&bs16, sizeof(FAT16BootSector), 1, infile);
            if(!noDebug){
                print16(&bs16);
            }
        }
        else{
            fread(&bs32, sizeof(FAT32BootSector), 1, infile);        
            if(!noDebug){
                print32(&bs32);
            }
        }
        
        //read the root directories
        if(found16){
            /* -1 since we already parsed the boot, and the cursor is advanced some.
                SEEK_CUR flag means read from the present position of the cursor            
            */
            fseek(infile, ((bs16.rsvdSecCnt - 1) + bs16.FAT16Sz * bs16.numFats)*bs16.sectorSize, SEEK_CUR);
            FATEntry entry;
            for(i=0; i<bs16.rootEntCnt; i++){
                fread(&entry, sizeof (entry), 1, infile);
                printDir(&entry);
            }
        }
        else{
            FirstDataSec = bs32.rsvdSecCnt + (bs32.numFats * bs32.FAT32Sz);
            FirstSecOfCluster = ((bs32.rootClus - 2)*bs32.secPerClus) + FirstDataSec;
        
            printf("Sanity check: %d FATS x %d sectors per FAT + %d additional reserved sectors = %d\n", bs32.numFats, bs32.FAT32Sz, bs32.rsvdSecCnt, ((bs32.numFats * bs32.FAT32Sz) + bs32.rsvdSecCnt));
            printf("Checking root at sector %d\n", FirstDataSec);
            /* For FAT32, we have this field and can jump directly to it */
            fseek(infile, 0, SEEK_SET);
            fseek(infile, (FirstDataSec * bs32.sectorSize), SEEK_SET);

            /* TODO figure out why this is incorrect - Seems to be that the
            directories are slightly offset from one another - possibly missed
            a field?
            */
            
            FATEntry entry;
            for(i = 0; i<8; i++){
                fread(&entry, sizeof (entry), 1, infile);
                printDir(&entry);
            }
            printf("Done\n");
        }
    }
    else{
        printf("Parsing MBR...\n");
        /* Go to start of partition table and populate our partitions all at once
        (4 records) */
        fseek(infile, 0x1BE, SEEK_SET);
        fread(partitions, sizeof(PartitionTable), 4, infile);
        
        //Check what we are looking at
        for(i=0; i<4; i++) {        
            if(partitions[i].partitionType == 4 || partitions[i].partitionType == 6 ||
               partitions[i].partitionType == 14) {
                printf("FAT16 filesystem found from partition %d\n", i);
                found16 = 1;
                break;
            }
            if(partitions[i].partitionType == 11 || partitions[i].partitionType == 12){
                printf("FAT32 filesystem found from partition %d\n", i);
                found32 = 1;
                break;
            }
        }
        if(i == 4) {
            printf("No FAT filesystem found, exiting...\n");
            return -1;
        }
        
        printf("Parsing Boot Sector...\n");
        //Now that we know what to look for and where, get the metadata
        fseek(infile, 512 * partitions[i].startSector, SEEK_SET);
        if(found16){
            fread(&bs16, sizeof(FAT16BootSector), 1, infile);
            if(!noDebug){
                printPartitions(partitions);
                print16(&bs16);
            }
        }
        else{
            fread(&bs32, sizeof(FAT32BootSector), 1, infile);        
            if(!noDebug){
                printPartitions(partitions);
                print32(&bs32);
            }
        }
        
        
        printf("Parsing root directories...\n");
        //read the root directories
        if(found16){
            /* -1 since we already parsed the boot, and the cursor is advanced some*/
            fseek(infile, ((bs16.rsvdSecCnt - 1) + bs16.FAT16Sz * bs16.numFats)*bs16.sectorSize, SEEK_CUR);
            FATEntry entry;
            for(i=0; i<bs16.rootEntCnt; i++){
                fread(&entry, sizeof (entry), 1, infile);
                printDir(&entry);
            }
        }
        else{
            /* For FAT32, we have this field and can jump directly to it */
            fseek(infile, bs32.rootClus, SEEK_SET);
            //do nothing for now
        }
    }
   
}

void printPartitions(PartitionTable* pt){
    int i;
    for(i=0; i<4; i++){
        printf("Partition %d, type %02X\n", i, pt[i].partitionType);
        printf("Start sector %08X, length %d secotors\n\n", pt[i].startSector, 
           pt[i].lengthInSectors);
    }
}

void print16(FAT16BootSector* bs){
    printf("  Jump code: %02X:%02X:%02X\n", bs->jumpCode[0], bs->jumpCode[1], bs->jumpCode[2]);
    printf("  OEM code: [%.8s]\n", bs->oem);
    printf("  sector_size: %d\n", bs->sectorSize);
    printf("  sectors_per_cluster: %d\n", bs->secPerClus);
    printf("  reserved_sectors: %d\n", bs->rsvdSecCnt);
    printf("  number_of_fats: %d\n", bs->numFats);
    printf("  root_dir_entries: %d\n", bs->rootEntCnt);
    printf("  total_sectors_short: %d\n", bs->totalSec16);
    printf("  media_descriptor: 0x%02X\n", bs->mediaDesc);
    printf("  fat16_size_sectors: %d\n", bs->FAT16Sz);
    printf("  sectors_per_track: %d\n", bs->secPerTrack);
    printf("  number_of_heads: %d\n", bs->numHeads);
    printf("  hidden_sectors: %d\n", bs->illuminati);
    printf("  total_sectors_long: %d\n", bs->totalSec32);
    printf("  drive_number: 0x%02X\n", bs->driveNum);
    printf("  current_head: 0x%02X\n", bs->itsTradition);
    printf("  boot_signature: 0x%02X\n", bs->bootSig);
    printf("  volume_id: 0x%08X\n", bs->volumeID);
    printf("  Volume label: [%.11s]\n", bs->volumeLabel);
    printf("  Filesystem type: [%.8s]\n", bs->fileSysType);
}

void print32(FAT32BootSector* bs){
    printf("  jump code: %02X:%02X:%02X\n", bs->jumpCode[0], bs->jumpCode[1], bs->jumpCode[2]);
    printf("  OEM code: [%.8s]\n", bs->oem);
    printf("  sector_size: %d\n", bs->sectorSize);
    printf("  sectors_per_cluster: %d\n", bs->secPerClus);
    printf("  reserved_sectors: %d\n", bs->rsvdSecCnt);
    printf("  number_of_fats: %d\n", bs->numFats);
    printf("  root_dir_entries: %d\n", bs->rootEntCnt);
    printf("  total_sectors_short: %d\n", bs->totalSec16);
    printf("  media_descriptor: 0x%02X\n", bs->mediaDesc);
    printf("  fat16_size_sectors: %d\n", bs->FAT16Sz);
    printf("  sectors_per_track: %d\n", bs->secPerTrack);
    printf("  number_of_heads: %d\n", bs->numHeads);
    printf("  hidden_sectors: %d\n", bs->illuminati);
    printf("  total_sectors_long: %d\n", bs->totalSec32);
    printf("  fat32_size_sectors: %d\n", bs->FAT32Sz);
    printf("  extended FAT flags: 0x%02X\n", bs->extFlags);
    printf("  root cluster start: %08X\n", bs->rootClus);
}

int compareStr(char* str1, char* str2){
    int c1, c2, i;
    c1=c2=i=0;
    while(*(str1+i) != '\0'){
        if(*(str1 + i) != *(str2 + i)){
            return 0;
        }
        if(i>500){
            return 0;
        }
        i++;
    }
    return 1;
}

void printDir(FATEntry* entry){
    switch(entry->filename[0]) {
    case 0x00:
        return; // unused entry
    case 0xE5:
        printf("Deleted file: [?%.7s.%.3s]\n", entry->filename+1, entry->fileExt);
        return;
    case 0x05:
        printf("File starting with 0xE5: [%c%.7s.%.3s]\n", 0xE5, entry->filename+1, entry->fileExt);
        break;
    case 0x2E:
        printf("Directory: [%.8s.%.3s]\n", entry->filename, entry->fileExt);
        break;
    default:
        printf("File: [%.8s.%.3s]\n", entry->filename, entry->fileExt);
    }

    printf("  Modified: %04d-%02d-%02d %02d:%02d.%02d    Start: [%04X]    Size: %d\n", 
        1980 + (entry->date >> 9), (entry->date >> 5) & 0xF, entry->date & 0x1F,
        (entry->time >> 11), (entry->time >> 5) & 0x3F, entry->time & 0x1F,
        entry->startCluster, entry->fileSize);
}
