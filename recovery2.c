#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <dirent.h> 
#include <fcntl.h>

//Copy from Sample Code
#pragma pack(push,1)
struct BootdirEntries
{
    unsigned char BS_jmpBoot[3];    /* Assembly instruction to jump to boot code */
    unsigned char BS_OEMName[8];    /* OEM Name in ASCII */
    unsigned short BPB_BytsPerSec; /* Bytes per sector. Allowed values include 512, 1024, 2048, and 4096 */
    unsigned char BPB_SecPerClus; /* Sectors per cluster (data unit). Allowed values are powers of 2, but the cluster size must be 32KB or smaller */
    unsigned short BPB_RsvdSecCnt;    /* Size in sectors of the reserved area */
    unsigned char BPB_NumFATs;    /* Number of FATs */
    unsigned short BPB_RootEntCnt; /* Maximum number of files in the root directory for FAT12 and FAT16. This is 0 for FAT32 */
    unsigned short BPB_TotSec16;    /* 16-bit value of number of sectors in file system */
    unsigned char BPB_Media;    /* Media type */
    unsigned short BPB_FATSz16; /* 16-bit size in sectors of each FAT for FAT12 and FAT16.  For FAT32, this field is 0 */
    unsigned short BPB_SecPerTrk;    /* Sectors per track of storage device */
    unsigned short BPB_NumHeads;    /* Number of heads in storage device */
    unsigned int BPB_HiddSec;    /* Number of sectors before the start of partition */
    unsigned int BPB_TotSec32; /* 32-bit value of number of sectors in file system.  Either this value or the 16-bit value above must be 0 */
    unsigned int BPB_FATSz32;    /* 32-bit size in sectors of one FAT */
    unsigned short BPB_ExtFlags;    /* A flag for FAT */
    unsigned short BPB_FSVer;    /* The major and minor version number */
	unsigned int BPB_RootClus;    /* Cluster where the root directory can be found */
    unsigned short BPB_FSInfo;    /* Sector where FSINFO structure can be found */
    unsigned short BPB_BkBootSec;    /* Sector where backup copy of boot sector is located */
    unsigned char BPB_Reserved[12];    /* Reserved */
    unsigned char BS_DrvNum;    /* BIOS INT13h drive number */
    unsigned char BS_Reserved1;    /* Not used */
    unsigned char BS_BootSig; /* Extended boot signature to identify if the next three values are valid */
    unsigned int BS_VolID;    /* Volume serial number */
    unsigned char BS_VolLab[11]; /* Volume label in ASCII. User defines when creating the file system */
    unsigned char BS_FilSysType[8];    /* File system type label in ASCII */
};

struct DirdirEntries
{
    unsigned char DIR_Name[11];
    unsigned char DIR_Attr;
    unsigned char DIR_NTRes;
    unsigned char DIR_CrtTimeTenth;
    unsigned short DIR_CrtTime;
    unsigned short DIR_CrtDate;
    unsigned short DIR_LstAccDate;
    unsigned short DIR_FstClusHI;
    unsigned short DIR_WrtTime;
    unsigned short DIR_WrtDate;
    unsigned short DIR_FstClusLO;
    unsigned int DIR_FileSize;
};

#pragma pack(pop)

/* global var */
struct BootdirEntries *be;
unsigned int *fat;
unsigned int cluster_total;
struct DirdirEntries *dirEntries;
unsigned int numDirEntries;
unsigned int dataOffset;
unsigned int cluster_size;

void printUsage() {
    printf("Usage: ./recover -d [device filename] [other arguments]\n");
    printf("-l                   List the target directory\n");
    printf("-r target -o dest    Recover the target pathname\n");
}

void init(char* diskPath) {
	// get boot dirEntries
	FILE *fp = fopen(diskPath, "r");
	be = malloc(sizeof(struct BootdirEntries));
	fread(be, sizeof(struct BootdirEntries), 1, fp);

	// get fat table
	unsigned int fatSizeInByte = (unsigned int)(be->BPB_FATSz32 * be->BPB_BytsPerSec);
	cluster_size = be->BPB_BytsPerSec * be->BPB_SecPerClus;
	cluster_total = (be->BPB_TotSec32 - (be->BPB_NumFATs * be->BPB_FATSz32) - be->BPB_RsvdSecCnt)/ be->BPB_SecPerClus;
	fat = malloc(fatSizeInByte);
	pread(fileno(fp), fat, fatSizeInByte,(unsigned int)(be->BPB_RsvdSecCnt * be->BPB_BytsPerSec));
	dataOffset = (be->BPB_RsvdSecCnt + be->BPB_NumFATs * be->BPB_FATSz32) * be->BPB_BytsPerSec;

	fclose(fp);
}

void printfile(char* diskPath, int t) {

	FILE *fp = fopen(diskPath, "r");
	// calc root dir dirEntries span
	unsigned int i;
	unsigned int rootSpan = 0;
	struct DirdirEntries *p;
	for (i=t; i<0x0FFFFFF7; i=fat[i])
		rootSpan++;
	//printf("%d\n",rootSpan);
	p = dirEntries = malloc(rootSpan * cluster_size);
	numDirEntries = rootSpan * cluster_size / sizeof(struct DirdirEntries);

	// get all directory entries
	for (i=t; i<0x0FFFFFF7; i=fat[i]) {
		pread(fileno(fp), p, cluster_size, dataOffset + (i-2)*cluster_size);
		p += cluster_size/sizeof(struct DirdirEntries);
	}
    fclose(fp);
}

void showBootSectorInfo() {
    printf("Number of FATs = %u\n", be->BPB_NumFATs);
    printf("Numeber of bytes per sector = %u\n", be->BPB_BytsPerSec);
    printf("Numeber of sectors per cluster = %u\n", be->BPB_SecPerClus);
    printf("Numeber of reserved sectors = %u\n", be->BPB_RsvdSecCnt);

    int i = 0;
    int allocated = 0;
    for(i = 2;i < cluster_total+2; i++){
        if(fat[i]){
			allocated++;
        }
    }

    printf("Number of allocated clusters = %u\n", allocated);
    printf("Number of free clusters = %u\n", cluster_total-allocated);
}

void getsname(int index, char *buffer){
    int i;
    for(i=0;i<8;i++){
		if (dirEntries[index].DIR_Name[i] == ' ') break;
		*buffer++ = dirEntries[index].DIR_Name[i];
    }
	if (dirEntries[index].DIR_Name[8] != ' ') {
		*buffer++ = '.';
		for(i=8;i<=10;i++){
			if (dirEntries[index].DIR_Name[i] == ' ') break;
			*buffer++ = dirEntries[index].DIR_Name[i];
        }
	}
	if (dirEntries[index].DIR_Attr & 0x10)
		*buffer++ = '/';

	*buffer = 0;
}

char **tok_input(char* input) {
	int position = 0;
	int bufsize = 1024;
 	char **tokens = malloc(bufsize * sizeof(char*));
   	char *token;
   	token = strtok(input, "/");

   	while (token != NULL) {
    	tokens[position] = token;
    	//printf("%s\n", tokens[position]);
    	position++;
		token = strtok(NULL, "/");
   	}
   	tokens[position] = NULL;
   	return tokens;
}

void listDirectory(char* diskPath, char* path) {
	char **args;
	char string[257];
	args = tok_input(path);
	printfile(diskPath, be->BPB_RootClus);
	int i = 0; 
	int j,k;
	unsigned int count = 1;
	char sname[257];
	//printf("start");
	while (args[i] !=NULL){
		strcpy(string,args[i]);
		strcat(string,"/");
		//printf("%s\n",string);
		for (j=0; j<numDirEntries; j++) {
			if (dirEntries[j].DIR_Name[0] == 0 || dirEntries[j].DIR_Attr == 0x0f)
				continue;
			getsname(j, sname);
			if (strcmp(sname,string) ==0){
				int clu_no = (int) (dirEntries[j].DIR_FstClusHI<<16)+dirEntries[j].DIR_FstClusLO;
				//printf("%d\n", clu_no);
				printfile(diskPath, clu_no);
			}
		}
		i++;
	}
	if (args[i] == NULL) {
		for (k=0; k<numDirEntries; k++) {
			if (dirEntries[k].DIR_Name[0] == 0 || dirEntries[k].DIR_Attr == 0x0f)
				continue;
			getsname(k, sname);
			//printf("print now");
			printf("%u, %s", count++, sname);
			printf(", %u, %u\n", dirEntries[k].DIR_FileSize, (dirEntries[k].DIR_FstClusHI<<16)+dirEntries[k].DIR_FstClusLO);
		}
	}
	free(args);
}

void recoverShortfile(char* diskPath,char target[256],char outputtarget[256]) {
	int i,index,count=0,find = 0;
	char *buffer;
	char temptarget[256];
	unsigned int FirstClus;
	char *ReadData;
	FILE *fPtr;
	FILE *fp = fopen(diskPath, "r");
	printfile(diskPath, be->BPB_RootClus);

	for(i=1;i<256;i++){
		temptarget[count++] = target[i];
	}

	for (index=0; index<numDirEntries ;index++){
		buffer = malloc(13);
		count = 0;
		if(dirEntries[index].DIR_Name[0] == 0xe5){
		for(i=1;i<8;i++){
			if (dirEntries[index].DIR_Name[i] == ' ') break;
			buffer[count] = dirEntries[index].DIR_Name[i];
			count++;
		}
		if (dirEntries[index].DIR_Name[8] != ' ') {
			buffer[count] = '.';
			count++;
			for(i=8;i<=10;i++){
				if (dirEntries[index].DIR_Name[i] == ' ') break;
				buffer[count] = dirEntries[index].DIR_Name[i];
				count++;
			}
		}
		buffer[count] = 0;
		}
		if(strcmp(buffer , temptarget) == 0){
		find++;
		}
	}

	if(find > 1){
		printf("%s: error - ambiguous\n",target);
		return;
	}

	find = 0;

	for (index=0; index<numDirEntries ;index++){
		buffer = malloc(13);
		count = 0;
		if(dirEntries[index].DIR_Name[0] == 0xe5){
		// printf("I find a deleted file >.<\n");
		for(i=1;i<8;i++){
			if (dirEntries[index].DIR_Name[i] == ' ') break;
			buffer[count] = dirEntries[index].DIR_Name[i];
			count++;
		}
		if (dirEntries[index].DIR_Name[8] != ' ') {
			buffer[count] = '.';
			count++;
			for(i=8;i<=10;i++){
				if (dirEntries[index].DIR_Name[i] == ' ') break;
				buffer[count] = dirEntries[index].DIR_Name[i];
				count++;
			}
		}
		buffer[count] = 0;

		/* find deleted file same as input */
		if(strcmp(buffer , temptarget) == 0){
			find = 1;
			FirstClus=(dirEntries[index].DIR_FstClusHI<<16)+dirEntries[index].DIR_FstClusLO;
			//printf("%u\n",FirstClus);
			//printf("%x\n",fat[FirstClus]);
			if(fat[FirstClus] == 0){
				//printf("I can recovery.\n");
				ReadData = malloc(cluster_size);
				fPtr = fopen(outputtarget, "w");
					if(!fPtr){
					printf("%s: failed to open\n",outputtarget);
					// should break here
					break;
					}
				pread(fileno(fp), ReadData, cluster_size, dataOffset + (FirstClus-2)*cluster_size);
				fwrite (ReadData ,dirEntries[index].DIR_FileSize, 1, fPtr);
				fclose(fPtr);
				printf("%s: recovered\n",target);
			} else {
				printf("%s: error - fail to recover\n",target);
				break;
			}
		}


		}
		buffer = NULL;
		free(buffer);
	}

	if(find == 0){
		printf("%s: error - file not found\n",target);
	}

	fclose(fp);
}

int main(int argc, char *argv[]) {
	int opt;
  	if (argv <= 4){
    	printUsage();
  	}
    int option = 0;
    int dFlag = 0;
    int iFlag = 0;
    int lFlag = 0;
    int rFlag = 0;
    int RFlag = 0;
    int oFlag = 0;
    char diskPath[100];
    char targetPath[1024];
	char recovertarget[256];
	char outputtarget[256];
    while ((option = getopt(argc,argv,"d:il:r:R:o:"))!=-1) {
        switch (option) {
            case 'd':
                dFlag = 1;
                strcpy(diskPath, optarg);
                break;
            case 'i':
                iFlag = 1;
                break;
            case 'l':
                lFlag = 1;
                strcpy(targetPath, optarg);
                break;
            case 'r':
                rFlag = 1;
             //   printf("small r = recover with 8.3 filename, target: %s\n",optarg);
				strcpy(recovertarget,optarg);
                break;
            case 'R':
                RFlag = 1;
             //   printf("large R = recover with int filename, target: %s\n",optarg);
				strcpy(recovertarget,optarg);
                break;
            case 'o':
                oFlag = 1;
             //   printf("output to: %s\n",optarg);
				strcpy(outputtarget,optarg);
                break;
            default:
                printf("unrecognized argument: %c",option);
        }
    }
    if (dFlag + iFlag + lFlag + rFlag + RFlag + oFlag == 0) {
        printUsage();
		return 0;
    } else if (iFlag && lFlag) {
        // wrong usage = print usage
        printUsage();
		return 0;
    } else if (!iFlag && !lFlag && (!dFlag || !oFlag || (rFlag + RFlag != 1))) {
        // anyway wrong usage, fuck you
        printUsage();
		return 0;
    } else if (optind < argc) {
		printUsage();
		return 0;
	}

	// get all info first
	init(diskPath);
	//printf("init done");

    if (iFlag) {
        showBootSectorInfo();
    }
	if (lFlag) {
		listDirectory(diskPath,targetPath);
	}
	if (rFlag) {
		recoverShortfile(diskPath,recovertarget,outputtarget);
	}
	if (RFlag) {
		recoverintfile(diskPath,recovertarget,outputtarget);
	}
    return 0;
}