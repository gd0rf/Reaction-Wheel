/** @file cubeObc_bootloader.c
 *
 * @brief libCubeObc CubeSpace bootloader common operation helpers
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Cubespace ADCS All rights reserved.
 */

/***************************** SYSTEM INCLUDES *******************************/

/***************************** MODULE INCLUDES *******************************/

#include <cubeObc/cubeObc_bootloader.h>
#include <cubeObc/cubeObc_bulkDataTransfer.h>

/***************************** MODULE DEFINES ********************************/

/****************************** MODULE MACROS ********************************/

/***************************** MODULE TYPEDEFS *******************************/

/***************************** MODULE VARIABLES ******************************/

/***************************** MODULE FUNCTIONS ******************************/

/***************************** GLOBAL FUNCTIONS ******************************/

ErrorCode cubeObc_bootloader_pollForState(TypeDef_TctlmEndpoint *endpoint,
										  TypesCubeCommonBaseBootloader5_States state,
										  U32 backoff, U32 timeout, Boolean *error)
{
	ErrorCode result;
	Boolean done = FALSE;
	U16 backoffTotal = 0u; // How long we have waited in total for initialization to complete

	do
	{
		TypesCubeCommonBaseBootloader5_State appState;

		result = tctlmCubeCommonBaseBootloader5_getState(endpoint, &appState);

		if (result == CUBEOBC_ERROR_OK)
		{
			*error = (appState.result != CUBEOBC_ERROR_OK);

			done = ((appState.appState == state) || (appState.result != CUBEOBC_ERROR_OK));

			if (done == FALSE)
			{
				if (backoffTotal >= timeout)
				{
					result = CUBEOBC_ERROR_TOUT;
				}
				else
				{
					cubeObc_time_delay(backoff);

					backoffTotal += backoff; // Increment how many milliseconds we have waited
				}
			}
		}
	}
	while ((result == CUBEOBC_ERROR_OK) && (done == FALSE));

	return result;
}

ErrorCode cubeObc_bootloader_uploadCubeSpaceFile(TypeDef_TctlmEndpoint *endpoint, U32 size, void *userData,
												 TypesCubeCommonBaseBootloader5_Errors *errors)
{
	if ((endpoint == NULL) || (errors == NULL))
		return CUBEOBC_ERROR_NULLPTR;

	if (endpoint->nodeType == TYPES_COMMON_FRAMEWORK_TYPES_1__NODE_TYPE_INVALID)
		return CUBEOBC_ERROR_NODE_TYPE;

	ErrorCode result;
	U16 metaSize;
	U32 dataSize;
	TypesCubeCommonBaseBootloader5_WriteFileSetup setup;
	U8 *data;

	ZERO_VAR(*errors);

	// Request a new frame buffer from OBC - only 2 bytes to get the size of the meta data
	result = cubeObc_bulkDataTransfer_getFrameBuffer(userData, &data, sizeof(U16));

	if (result == CUBEOBC_ERROR_OK)
	{
		metaSize = *((U16 *)data); // The size of the meta data is in the first 2 bytes of the file
		dataSize = size - metaSize;

		// Request a new frame buffer from OBC - for the meta data
		// Note that we do not commit the previous frame so that we get this frame buffer also from the start of the file
		result = cubeObc_bulkDataTransfer_getFrameBuffer(userData, &data, metaSize);
	}

	if (result == CUBEOBC_ERROR_OK)
	{
		// Tell the OBC we have used "metaSize" frame and buffer index should be updated
		result = cubeObc_bulkDataTransfer_commitFrameBuffer(userData, data, metaSize);
	}

	if (result == CUBEOBC_ERROR_OK)
	{
		MEMCPY(setup.metaData, data, metaSize);

		result = tctlmCubeCommonBaseBootloader5_setWriteFileSetup(endpoint, &setup);
	}

	if (result == CUBEOBC_ERROR_OK)
	{
		Boolean error;

		// Upload to internal flash requires 30000ms to initialize to cater for all cases
		result = cubeObc_bootloader_pollForState(endpoint, TYPES_CUBE_COMMON_BASE_BOOTLOADER_5__STATE_BUSY_WAIT_FRAME,
												 200u, 30000u, &error);

		if ((result == CUBEOBC_ERROR_OK) &&
			(error == TRUE))
		{
			// Requests for <State> were successful, but there was an internal error
			// The caller should inspect <Errors> telemetry for details of the error
			(void)tctlmCubeCommonBaseBootloader5_getErrors(endpoint, errors);

			return CUBEOBC_ERROR_FTP;
		}
	}

	if (result == CUBEOBC_ERROR_OK)
	{
		// Only pass the data portion of the file to be uploaded
		result = cubeObc_bulkDataTransfer_upload(endpoint, userData, dataSize);

		// Request errors one final time after the upload to capture any errors that may have occurred
		(void)tctlmCubeCommonBaseBootloader5_getErrors(endpoint, errors);
	}

	return result;
}

ErrorCode cubeObc_bootloader_upgrade(TypeDef_TctlmEndpoint *endpoint, U32 size, void *userData,
									 U8 vMajor, U8 vMinor,
									 TypesCubeCommonBaseBootloader5_Errors *errors)
{
	if ((endpoint == NULL) || (errors == NULL))
		return CUBEOBC_ERROR_NULLPTR;

	if (endpoint->nodeType == TYPES_COMMON_FRAMEWORK_TYPES_1__NODE_TYPE_INVALID)
		return CUBEOBC_ERROR_NODE_TYPE;

	ErrorCode result;

	// Step 1. Upload new bootloader file (*-bin-upgrade.cs)
	result = cubeObc_bootloader_uploadCubeSpaceFile(endpoint, size, userData, errors);

	// Step 2. Request FileInfo telemetry until the bootloader binary file is located
	if (result == CUBEOBC_ERROR_OK)
	{
		result = tctlmCubeCommonBaseBootloader5_setResetFileInfoIdx(endpoint);

		if (result == CUBEOBC_ERROR_OK)
		{
			TypesCubeCommonBaseBootloader5_FileInfo fileInfo;
			Boolean done = FALSE;
			Boolean found = FALSE;

			do
			{
				result = tctlmCubeCommonBaseBootloader5_getFileInfo(endpoint, &fileInfo);

				if (result == CUBEOBC_ERROR_OK)
				{
					found = (fileInfo.program == TYPES_COMMON_FRAMEWORK_TYPES_1__PROGRAM_TYPE_BOOTLOADER);

					done = fileInfo.last || fileInfo.empty;
				}
			}
			while ((result == CUBEOBC_ERROR_OK) && (done == FALSE) && (found == FALSE));

			if ((result == CUBEOBC_ERROR_OK) && (found == FALSE))
			{
				result = CUBEOBC_ERROR_EXIST;
			}

			if (result == CUBEOBC_ERROR_OK)
			{
				// Confirm parameters
				if ((fileInfo.isCorrupt == TRUE) ||
					(fileInfo.address != 0x8100000u) ||
					(fileInfo.firmwareMajorVersion != vMajor) ||
					(fileInfo.firmwareMinorVersion != vMinor) ||
					(fileInfo.firmwarePatchVersion != 0u))
				{
					result = CUBEOBC_ERROR_UNKNOWN;
				}
			}
		}
	}

	// Step 3. Request OptionBytes and store for later modification
	TypesCubeCommonBaseBootloader5_OptionBytes optionBytes;

	if (result == CUBEOBC_ERROR_OK)
	{
		result = tctlmCubeCommonBaseBootloader5_getOptionBytes(endpoint, &optionBytes);
	}

	// Step 4. Modify OptionBytes to change the boot bank
	// Note: This step also handles Step 13 in the documentation
	if (result == CUBEOBC_ERROR_OK)
	{
		if (optionBytes.bfb2 == FALSE)
		{
			// Step 4
			optionBytes.bfb2 = TRUE; // Swap to boot from bank 2
			optionBytes.nSwBoot0 = FALSE;
			// Remove write protection of old bootloader in bank 1
			optionBytes.wrpAStrt1 = 255;
			optionBytes.wrpAEnd1 = 0;
			// Write protect the new bootloader in bank 2
			optionBytes.wrpAStrt2 = 0;
			optionBytes.wrpAEnd2 = 41;
		}
		else
		{
			// Step 13
			optionBytes.bfb2 = FALSE; // Swap to boot from bank 1
			optionBytes.nSwBoot0 = TRUE; // May be left as FALSE if user has access to CAN1, UART1, or I2C
			// Write protect the new bootloader in bank 1
			optionBytes.wrpAStrt1 = 0;
			optionBytes.wrpAEnd1 = 41;
			// Remove write protection of old bootloader in bank 2
			optionBytes.wrpAStrt2 = 255;
			optionBytes.wrpAEnd2 = 0;
		}
	}

	// Step 5. Send the option bytes, then request again to confirm values
	// N.B! CubeComputer must have stable power and must not be reset during this step
	if (result == CUBEOBC_ERROR_OK)
	{
		result = tctlmCubeCommonBaseBootloader5_setOptionBytes(endpoint, &optionBytes);

		if (result == CUBEOBC_ERROR_OK)
		{
			TypesCubeCommonBaseBootloader5_OptionBytes optionBytesConfirm;

			result = tctlmCubeCommonBaseBootloader5_getOptionBytes(endpoint, &optionBytesConfirm);

			if (result == CUBEOBC_ERROR_OK)
			{
				if ((optionBytes.bfb2 != optionBytesConfirm.bfb2) ||
					(optionBytes.nSwBoot0 != optionBytesConfirm.nSwBoot0) ||
					(optionBytes.wrpAStrt1 != optionBytesConfirm.wrpAStrt1) ||
					(optionBytes.wrpAEnd1 != optionBytesConfirm.wrpAEnd1) ||
					(optionBytes.wrpAStrt2 != optionBytesConfirm.wrpAStrt2) ||
					(optionBytes.wrpAEnd2 != optionBytesConfirm.wrpAEnd2))
				{
					result = CUBEOBC_ERROR_WRITE;
				}
			}
		}
	}

	// Step 6. Commit the option bytes
	// N.B! CubeComputer must have stable power and must not be reset during this step
	if (result == CUBEOBC_ERROR_OK)
	{
		TypesCubeCommonBaseBootloader5_CommitOptionBytes commit;

		commit.magicNumber = optionBytes.magicNumber;

		result = tctlmCubeCommonBaseBootloader5_setCommitOptionBytes(endpoint, &commit);

		if (result == CUBEOBC_ERROR_OK)
		{
			cubeObc_time_delay(100u);

			result = tctlmCubeCommonBaseBootloader5_setHalt(endpoint);
		}
	}

	// Step 7. Confirm memory mapping matches the selected boot bank
	if (result == CUBEOBC_ERROR_OK)
	{
		TypesCubeCommonBaseBootloader5_OptionBytes optionBytesConfirm;

		result = tctlmCubeCommonBaseBootloader5_getOptionBytes(endpoint, &optionBytesConfirm);

		if (result == CUBEOBC_ERROR_OK)
		{
			TypesCubeCommonBaseBootloader5_MemMap memmap;

			result = tctlmCubeCommonBaseBootloader5_getMemMap(endpoint, &memmap);

			if (result == CUBEOBC_ERROR_OK)
			{
				// Highly unlikely scenario
				if (optionBytesConfirm.bfb2 != memmap.memrmp)
				{
					result = CUBEOBC_ERROR_UNKNOWN;

					if (optionBytes.bfb2 == FALSE)
					{
						// Theoretically impossible, as there would be no response from the bootloader after step 6.
					}
					else
					{
						// The MCU was commanded to boot from bank 2
						// but the old bootloader in bank 1 is the one that is executing.
						// In this case the upgrade would need to be retried, following consultation with CubeSpace.
					}
				}
			}
		}
	}

	// Step 8. Confirm new bootloader is running with Identification
	if (result == CUBEOBC_ERROR_OK)
	{
		TypesCubeCommonBaseBootloader5_Identification id;

		result = tctlmCubeCommonBaseBootloader5_getIdentification(endpoint, &id);

		if (result == CUBEOBC_ERROR_OK)
		{
			if ((id.firmwareMajorVersion != vMajor) ||
				(id.firmwareMinorVersion != vMinor) ||
				(id.runtimeSeconds > 1u))
			{
				result = CUBEOBC_ERROR_UNKNOWN;
			}
		}
	}

	return result;
}

// TODO Raw memory access

/*** end of file ***/
