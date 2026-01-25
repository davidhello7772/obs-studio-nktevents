/*
 * Audio Monitor Plugin - Entry Point
 * Opens a separate window to monitor audio levels of all active audio sources
 */

#include "plugin-main.hpp"
#include "audio-monitor-window.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QAction>
#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("audio-monitor", "en-US")

static AudioMonitorWindow *monitorWindow = nullptr;

static void SaveMonitorData(obs_data_t *save_data, bool saving, void *)
{
	if (monitorWindow)
		monitorWindow->SaveLoadColorSettings(save_data, saving);
}

static void OnFrontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		// Colors will be loaded via SaveMonitorData callback
		// Just need to refresh the source list
		if (monitorWindow)
			monitorWindow->RefreshSources();
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		delete monitorWindow;
		monitorWindow = nullptr;
	}
}

bool obs_module_load(void)
{
	// Get main window for parenting
	obs_frontend_push_ui_translation(obs_module_get_string);
	QMainWindow *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	// Create the monitor window
	monitorWindow = new AudioMonitorWindow(mainWindow);
	obs_frontend_pop_ui_translation();

	// Add to Tools menu
	const char *menuText = obs_module_text("AudioMonitor.Menu");
	QAction *menuAction = (QAction *)obs_frontend_add_tools_menu_qaction(menuText);
	QObject::connect(menuAction, &QAction::triggered, []() { monitorWindow->ToggleShowHide(); });

	// Register save callback for persistent color settings
	obs_frontend_add_save_callback(SaveMonitorData, nullptr);
	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);

	return true;
}

void obs_module_unload(void)
{
	// Remove callbacks first
	obs_frontend_remove_save_callback(SaveMonitorData, nullptr);
	obs_frontend_remove_event_callback(OnFrontendEvent, nullptr);

	// Delete window (triggers all cleanup)
	delete monitorWindow;
	monitorWindow = nullptr;
}
