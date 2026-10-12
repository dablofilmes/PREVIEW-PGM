
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static void abrir_preview_pgm(void *data)
{
    UNUSED_PARAMETER(data);

    /*
     * Abre o projetor da saída de programa.
     * Monitor 1 normalmente corresponde ao segundo monitor.
     */
    obs_frontend_open_projector("StudioProgram", 1, NULL, NULL);
}

bool obs_module_load(void)
{
    obs_log(LOG_INFO, "PREVIEW PGM carregado - versao %s", PLUGIN_VERSION);

    obs_frontend_add_tools_menu_item(
        "Abrir PREVIEW PGM (monitor 2)",
        abrir_preview_pgm,
        NULL
    );

    return true;
}

void obs_module_unload(void)
{
    obs_log(LOG_INFO, "PREVIEW PGM descarregado");
}
