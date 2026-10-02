#include <common.h>
#include <command.h>
#include <asm/global_data.h>

extern void host_service_manager_main(void);

static int init_iNIC_process(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
#ifdef TCSUPPORT_HOST_SERVICE_MANAGER
	char *str = NULL;
	str = env_get("serdes_pon");
	printf("get serdes pon %s\n", str);

	if(str[0]!='7')
	{
		printf("serdes pon config error, please set to DSL mode...\n");
		return 0;
	}

	

    host_service_manager_main();
	return 0;
#else
	printf("TCSUPPORT_HOST_SERVICE_MANAGER if you want to use inic opt, please turn on the compile option...\n");
	return 0;
#endif
}

U_BOOT_CMD(
	init_iNIC,
	1,
	0,
	init_iNIC_process,
	"Initial iNIC process",
	""
)