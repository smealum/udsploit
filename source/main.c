#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include <3ds.h>

Result udsploit();
Result hook_kernel();

int main(int argc, char **argv)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	Result ret = 0;

	ret = udsploit();
	printf("%08X\n", (unsigned int)ret);
	if(ret) goto fail;
	
	printf("udsploit success\n");

	ret = hook_kernel();
	printf("%08X\n", (unsigned int)ret);
	if(ret) goto fail;

	fail:
	// Main loop
	while (aptMainLoop()) {

		gspWaitForVBlank();
		hidScanInput();

		// Your code goes here

		u32 kDown = hidKeysDown();
		if (kDown & KEY_START) break; // break in order to return to hbmenu

		// Flush and swap framebuffers
		gfxFlushBuffers();
		gfxSwapBuffers();
	}

	gfxExit();
	return 0;
}
