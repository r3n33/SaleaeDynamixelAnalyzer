#ifndef DYNAMIXEL_ANALYZER_SETTINGS
#define DYNAMIXEL_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

enum {SERVO_TYPE_AX, SERVO_TYPE_MX, SERVO_TYPE_XL};
enum {CONTROLLER_UNKNOWN=0xff, CONTROLLER_USB2AX=0xfd, CONTROLLER_CM730ISH=200};
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
	U32 mServoType;
	bool mShowWords;
	U32 mServoControllerID;

protected:
	std::auto_ptr< AnalyzerSettingInterfaceChannel >	mInputChannelInterface;
	std::auto_ptr< AnalyzerSettingInterfaceInteger >	mBitRateInterface;
	std::auto_ptr< AnalyzerSettingInterfaceNumberList >	mServoTypeInterface;
	std::auto_ptr< AnalyzerSettingInterfaceNumberList >	mServoControllerInterface;
	std::auto_ptr< AnalyzerSettingInterfaceBool >	    mShowWordsInterface;

};

#endif //DYNAMIXEL_ANALYZER_SETTINGS
