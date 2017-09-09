#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <tss/tss_error.h>
#include <tss/platform.h>
#include <tss/tss_defines.h>
#include <tss/tss_typedef.h>
#include <tss/tss_structs.h>
#include <tss/tspi.h>
#include <trousers/trousers.h>

#define Debug(message, tResult) {\
		printf("%s : %s\n", message, (char *)Trspi_Error_String(tResult)); \
		if (tResult != 0) {\
			return -1; \
		}}
void printMenu();

int main(int argc, char **argv)
{
    TSS_HCONTEXT    hContext;
    TSS_HTPM        hTPM;
    TSS_HPCRS       hPcrs;
    TSS_HENCDATA    hEncData;
    TSS_HENCDATA    hRetrieveData;
    TSS_RESULT      result;
    TSS_HKEY        hSRK = 0;
    TSS_HPOLICY     hSRKPolicy = 0;
    TSS_UUID        SRK_UUID = TSS_UUID_SRK;

    BYTE            wks[20];
    BYTE            *pubKey;
    UINT32          pubKeySize;
    BYTE            *rgbPcrValue;
    UINT32          ulPcrLen;
    BYTE            *encData;
    UINT32          encDataSize;
    BYTE            *outstring;
    UINT32          outlength;
    FILE            *fout, *fin;
    int             i;
    UINT32          j;
    BYTE            valueToExtend[250];
    int             count = 0;
    int             pcrToExtend = 0;


    memset(wks, 0, 20);
    memset(valueToExtend, 0, 250);

    //Pick the TPM you are talking to.
    //In this case, it is the system TPM(indicated with NULL)
    result = Tspi_Context_Create(&hContext);
    Debug("Create Context", result);

    result = Tspi_Context_Connect(hContext, NULL);
    Debug("Context Connect", result);

    //Get the TPM handle
    result = Tspi_Context_GetTpmObject(hContext, &hTPM);
    Debug("Get TPM Handle", result);

    //Get the SRK handle
    result = Tspi_Context_LoadKeyByUUID(hContext, TSS_PS_TYPE_SYSTEM, SRK_UUID, &hSRK);
    Debug("Get the SRK handle", result);

    //Get the SRK policy
    result = Tspi_GetPolicyObject(hSRK, TSS_POLICY_USAGE, &hSRKPolicy);
    Debug("Get the SRK policy", result);

    //Then set the SRK policy to be the well known secret
    result = Tspi_Policy_SetSecret(hSRKPolicy, TSS_SECRET_MODE_SHA1, 20, wks);


    //输出所有PCR寄存器内的值
    /*********************/
    for (j = 0; j < 24; j++)
    {
        result = Tspi_TPM_PcrRead(hTPM, j, &ulPcrLen, &rgbPcrValue);
        printf("PCR %02d ", j);
        for (i = 0; i < 19; i++)
            printf("%02x", *(rgbPcrValue + i));
        printf("\n");
    }
    /*********************/

    //Display each command line argument.
    printf("\n Command line arguments:\n");
    for (count = 0; count <argc; count++)
        printf("argv[%d] : %s\n", count, argv[count]);

    //Examine command line arguments.
    if (argc >= 3)
    {
        if (strcmp(argv[1],"-p") == 0)
        {
            pcrToExtend = atoi(argv[2]);
            if (pcrToExtend < 0 || pcrToExtend > 23)
            {
                printMenu();
                return 0;
            }
        }

        if (argc == 5)
        {
            if (strcmp(argv[3], "-v") == 0)
                memcpy(valueToExtend, argv[4], strlen(argv[4]));
        }
        else
        {
            printMenu();
            return 0;
        }
    }
    else
    {
        printMenu();
        return 0;
    }

    //Extend the value
    result = Tspi_TPM_PcrExtend(hTPM, pcrToExtend, 20, (BYTE *)valueToExtend, NULL, &ulPcrLen, &rgbPcrValue);
    Debug("Extended the PCR", result);

    //输出所有PCR寄存器内的值
    /*********************/
    for (j = 0; j < 24; j++)
    {
        result = Tspi_TPM_PcrRead(hTPM, j, &ulPcrLen, &rgbPcrValue);
        printf("PCR %02d ", j);
        for (i = 0; i < 19; i++)
            printf("%02x", *(rgbPcrValue + i));
        printf("\n");
    }
    /*********************/


    //Clean up
    Tspi_Context_FreeMemory(hContext, NULL);
    Tspi_Context_Close(hContext);

    return 0;
}

void printMenu()
{
    printf("\nChangePCRn Help Menu:\n");
    printf("\t -p PCR regiter to extend(0-23)\n");
    printf("\t -v Value to be extended into PCR(abc...)\n");
    printf("\t Note: -v argument is optional and a default value will be used if no value is provided\n");
    printf("\t Example: ChangePCRn -p 10 -v abcdef\n");
}
