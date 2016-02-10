#ifndef DYNAMIXEL_ANALYZER_SETTINGS
#define DYNAMIXEL_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

class DynamixelAnalyzerSettings : public AnalyzerSettings
{
public:
	DynamixelAnalyzerSettings();
	virtual ~DynamixelAnalyzerSettings();

	virtual bool SetSettingsFromInterfaces();
	void UpdateInterfacesFromSettings();
	virtual void LoadSettings( const char* settings );
	virtual const char* SaveSettings();

	
	Channel mInputChannel;
	U32 mBitRate;

protected:
	std::auto_ptr< AnalyzerSettingInterfaceChannel >	mInputChannelInterface;
	std::auto_ptr< AnalyzerSettingInterfaceInteger >	mBitRateInterface;
};

#endif //DYNAMIXEL_ANALYZER_SETTINGS
