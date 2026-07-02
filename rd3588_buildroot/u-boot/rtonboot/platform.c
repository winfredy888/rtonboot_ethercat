#include <common.h>
#include <rtonboot/rtonboot.h>

#define CONFIG_RTONBOOT_IMAGE_LOAD_ADDR 0xecc00000

void testit(void)
{
	//#ifndef USE_RTOS_PRINT
	#if 0

		printf("666666\n");

	#else

		rtos_printf("6666\n");

	#endif
}

void testswi(void)
{
	//#ifndef USE_RTOS_PRINT
	#if 0

		printf("88888888887777777777777777777777\n");

	#else

		rtos_printf("88888888887777777777777777777777\n");

	#endif
}

extern uint64_t testreg; 

void testfail(void)
{
	//#ifndef USE_RTOS_PRINT
	#if 0

		printf("bbbbbbbbbbbbbbbbbbbbbbbb %llx\n", testreg);

	#else

		//testreg = MMU_CLEAR_TOP_VADDR;

		rtos_printf("bbbbbbbbbbbbbbbbbbbbbbbb %llx\n", testreg);

	#endif
}
