INCLUDES=-I../ -I../../guilib -I../linux -I../../lib

SRCS=AdvancedSettings.cpp \
     GUIDialogAudioSubtitleSettings.cpp \
     GUIDialogContentSettings.cpp \
     GUIDialogLockSettings.cpp \
     GUIDialogProfileSettings.cpp \
     GUIDialogSettings.cpp \
     GUIDialogVideoSettings.cpp \
     GUISettings.cpp \
     GUIWindowSettingsCategory.cpp \
     GUIWindowSettings.cpp \
     GUIWindowSettingsProfile.cpp \
     GUIWindowSettingsScreenCalibration.cpp \
     SettingsControls.cpp \
     Settings.cpp \
     VideoSettings.cpp \

LIB=settings.a

include ../../Makefile.include
-include $(patsubst %.cpp,%.P,$(patsubst %.c,%.P,$(SRCS)))
