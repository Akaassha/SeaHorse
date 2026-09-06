// Fill out your copyright notice in the Description page of Project Settings.


#include "Frontend/FrontendGameplayTags.h"

namespace FrontendGameplayTags 
{
	//Frontend widget stack
	UE_DEFINE_GAMEPLAY_TAG(Frontend_WidgetStack_Modal, "Frontend.WidgetStack.Modal");
	UE_DEFINE_GAMEPLAY_TAG(Frontend_WidgetStack_GameMenu, "Frontend.WidgetStack.GameMenu");
	UE_DEFINE_GAMEPLAY_TAG(Frontend_WidgetStack_GameHud, "Frontend.WidgetStack.GameHud");
	UE_DEFINE_GAMEPLAY_TAG(Frontend_WidgetStack_Frontend, "Frontend.WidgetStack.Frontend");

	//Frontend widgets
	UE_DEFINE_GAMEPLAY_TAG(Frontend_Widget_PressAnyKeyScreen, "Frontend.Widget.PressAnyKeyScreen");
	UE_DEFINE_GAMEPLAY_TAG(Frontend_Widget_MainMenuScreen, "Frontend.Widget.MainMenuScreen");
	UE_DEFINE_GAMEPLAY_TAG(Frontend_Widget_OptionsScreen, "Frontend.Widget.OptionsScreen");
	UE_DEFINE_GAMEPLAY_TAG(Frontend_Widget_ConfirmScreen, "Frontend.Widget.ConfirmScreen");

	UE_DEFINE_GAMEPLAY_TAG(Frontend_Widget_KeyRemapScreen, "Frontend.Widget.KeyRemapScreen");

	//Frontend Options Image
	UE_DEFINE_GAMEPLAY_TAG(Frontedn_Image_TestImage, "Frontend.Image.TestImage");
	UE_DEFINE_GAMEPLAY_TAG(Frontedn_Image_HUDVisibility, "Frontend.Image.HUDVisibility");
}
#include "Internationalization/StringTableRegistry.h"

namespace
{
const bool bRegisteredFrontendStrings = []()
{
	LOCTABLE_NEW("SeaHorseFrontend", "SeaHorse.Frontend");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_120_fps", "120 fps");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_30_fps", "30 fps");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_3d_resolution", "3d resolution");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_3d_resolution_description", "Adjust the internal rendering resolution. Lower values improve performance at the cost of image sharpness.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_60_fps", "60 fps");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_90_fps", "90 fps");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_accesibility_settings_tab", "Accessibility");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_advenced_graphic", "Advanced graphics");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_allow_background_audio", "Allow background audio");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_antialiasing_quality", "Antialiasing quality");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_antialiasing_quality_description", "Reduce jagged edges. Higher quality may reduce performance.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_audio_settings_tab", "Audio");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_borderless_window", "Borderless window");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_cinematic", "Cinematic");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_controll_settings_tab", "Controls");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_disabled", "Disabled");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_display", "Display");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_enabled", "Enabled");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_epic", "Epic");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_far", "Far");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_frame_rate_limt", "Frame rate limit");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_frame_rate_limt_description", "Limit frames per second, or select No limit.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_fullscreen_mode", "Fullscreen mode");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_gamepad", "Gamepad");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_gamepad_settings_tab", "Gamepad");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_gameplay_settings_tab", "General");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_gamma", "Gamma");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_gamma_description", "Adjust the brightness of midtones. The default gamma is 2.2.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_global_illumination", "Global illumination");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_global_illumination_description", "Adjust the quality of indirect lighting.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_graphics", "Graphics");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_high", "High");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_keyboard_and_mouse", "Keyboard and mouse");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_language", "Language");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_low", "Low");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_medium", "Medium");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_music_volume", "Music volume");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_music_volume_description", "Adjust music volume. ");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_near", "Near");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_no_limit", "No limit");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_overall_quality", "Overall quality");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_overall_quality_description", "Apply a graphics quality preset. Custom means individual settings differ.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_overall_volume", "Overall volume");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_overall_volume_description", "Adjust the volume of all game audio.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_post_processing_quality", "Post processing quality");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_post_processing_quality_description", "Adjust the quality of post processing effects.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_reflection_quality", "Reflection quality");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_reflection_quality_description", "Adjust reflection quality.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_screen_resolution", "Screen resolution");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_screen_resolution_description", "Choose the output resolution in fullscreen or windowed mode.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_shadow_quality", "Shadow quality");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_shadow_quality_description", "Adjust shadow detail and quality.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_sound", "Sound");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_sound_effects_volume", "Sound effects volume");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_sound_effects_volume_description", "Adjust sound effects volume. ");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_texture_quality", "Texture quality");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_texture_quality_description", "Adjust texture quality and memory use.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_v-sync", "V-sync");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_v-sync_description", "Synchronize rendering with the display to reduce screen tearing.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_very_far", "Very far");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_video_settings_tab", "Video");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_view_distance", "View distance");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_view_distance_description", "Adjust how far away objects remain visible.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_visual_effects_quality", "Visual effects quality");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_visual_effects_quality_description", "Adjust the quality of visual effects.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_volume", "Volume");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_window_mode", "Window mode");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_window_mode_description", "Choose fullscreen, borderless, or windowed display.");
	LOCTABLE_SETSTRING("SeaHorseFrontend", "_windowed", "Windowed");
	return true;
}();
}
