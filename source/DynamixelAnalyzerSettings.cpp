#include "DynamixelAnalyzerSettings.h"
#include <AnalyzerHelpers.h>


DynamixelAnalyzerSettings::DynamixelAnalyzerSettings()
:	mInputChannel( UNDEFINED_CHANNEL ),
	mBitRate( 1000000 ),
	mServoType (SERVO_TYPE_AX),
	mShowWords ( true )
{
	mInputChannelInterface.reset( new AnalyzerSettingInterfaceChannel() );
	mInputChannelInterface->SetTitleAndTooltip( "Dynamixel", "Standard Dynamixel Protocol" );
	mInputChannelInterface->SetChannel( mInputChannel );

	mBitRateInterface.reset( new AnalyzerSettingInterfaceInteger() );
	mBitRateInterface->SetTitleAndTooltip( "Bit Rate (Bits/S)",  "Specify the bit rate in bits per second." );
	mBitRateInterface->SetMax( 6000000 );
	mBitRateInterface->SetMin( 1 );
	mBitRateInterface->SetInteger( mBitRate );

	mServoTypeInterface.reset(new AnalyzerSettingInterfaceNumberList());
	mServoTypeInterface->SetTitleAndTooltip("", "Type of Servos.");
	mServoTypeInterface->AddNumber(SERVO_TYPE_AX, "AX Servos (default)", "");
	mServoTypeInterface->AddNumber(SERVO_TYPE_MX, "MX Servos", "");
	mServoTypeInterface->SetNumber(mServoType);

	mShowWordsInterface.reset(new AnalyzerSettingInterfaceBool());
	mShowWordsInterface->SetTitleAndTooltip("", "Show two registers Low/High are shown as a word.");
	mShowWordsInterface->SetCheckBoxText("Show L/W values as words");
	mShowWordsInterface->SetValue(mShowWords);



	AddInterface( mInputChannelInterface.get() );
	AddInterface( mBitRateInterface.get() );
	AddInterface( mServoTypeInterface.get() );
	AddInterface(mShowWordsInterface.get());

	AddExportOption(0, "Export as csv file");
	AddExportExtension(0, "csv", "csv");

	ClearChannels();
	AddChannel( mInputChannel, "Dynamixel", false );
}

DynamixelAnalyzerSettings::~DynamixelAnalyzerSettings()
{
}

bool DynamixelAnalyzerSettings::SetSettingsFromInterfaces()
{
	mInputChannel = mInputChannelInterface->GetChannel();
	mBitRate = mBitRateInterface->GetInteger();
	mServoType = (U32)mServoTypeInterface->GetNumber();
	mShowWords = mShowWordsInterface->GetValue();

	ClearChannels();
	AddChannel( mInputChannel, "Dynamixel Protocol", true );

	return true;
}

void DynamixelAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mInputChannelInterface->SetChannel( mInputChannel );
	mBitRateInterface->SetInteger( mBitRate );
	mServoTypeInterface->SetNumber( mServoType );
	mShowWordsInterface->SetValue(mShowWords);

}

void DynamixelAnalyzerSettings::LoadSettings( const char* settings )
{
	SimpleArchive text_archive;
	text_archive.SetString( settings );

	text_archive >> mInputChannel;
	text_archive >> mBitRate;
	text_archive >> mServoType;
	text_archive >> mShowWords;


	ClearChannels();
	AddChannel( mInputChannel, "Dynamixel Protocol", true );

	UpdateInterfacesFromSettings();
}

const char* DynamixelAnalyzerSettings::SaveSettings()
{
	SimpleArchive text_archive;

	text_archive << mInputChannel;
	text_archive << mBitRate;
	text_archive << mServoType;
	text_archive << mShowWords;



	return SetReturnString( text_archive.GetString() );
}
