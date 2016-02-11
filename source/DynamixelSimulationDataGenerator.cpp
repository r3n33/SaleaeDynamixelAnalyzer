#include "DynamixelSimulationDataGenerator.h"
#include "DynamixelAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

char dxl_packets[] = { 	0xff, 0xff, 0x01, 0x04, 0x02, 0x2b, 0x01, 0xcc, //Read temp of ID1
						0xff, 0xff, 0xfe, 0x04, 0x03, 0x03, 0x01, 0xf6, //Write ID1 to broadcast
						0xff, 0xff, 0x01, 0x02, 0x01, 0xFB,				//Ping ID1
						0xff, 0xff, 0x02, 0x02, 0x06, 0xf7				//Reset ID2 bad checksum
						,0xff, 0xff, 0x01, 0x04, 0x02, 0x00, 0x0a, 0xfe
					 };
DynamixelSimulationDataGenerator::DynamixelSimulationDataGenerator()
:	mSerialText( dxl_packets ),
	mStringIndex( 0 )
{
}

DynamixelSimulationDataGenerator::~DynamixelSimulationDataGenerator()
{
}

void DynamixelSimulationDataGenerator::Initialize( U32 simulation_sample_rate, DynamixelAnalyzerSettings* settings )
{
	mSimulationSampleRateHz = simulation_sample_rate;
	mSettings = settings;

	mSerialSimulationData.SetChannel( mSettings->mInputChannel );
	mSerialSimulationData.SetSampleRate( simulation_sample_rate );
	mSerialSimulationData.SetInitialBitState( BIT_HIGH );
}

U32 DynamixelSimulationDataGenerator::GenerateSimulationData( U64 largest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel )
{
	U64 adjusted_largest_sample_requested = AnalyzerHelpers::AdjustSimulationTargetSample( largest_sample_requested, sample_rate, mSimulationSampleRateHz );

	while( mSerialSimulationData.GetCurrentSampleNumber() < adjusted_largest_sample_requested )
	{
		CreateSerialByte();
	}

	*simulation_channel = &mSerialSimulationData;
	return 1;
}

void DynamixelSimulationDataGenerator::CreateSerialByte()
{
	U32 samples_per_bit = mSimulationSampleRateHz / mSettings->mBitRate;

	U8 byte = mSerialText[ mStringIndex ];
	mStringIndex++;
	if( mStringIndex == mSerialText.size() )
		mStringIndex = 0;

	//we're currenty high
	//let's move forward a little
	mSerialSimulationData.Advance( samples_per_bit * 10 );

	mSerialSimulationData.Transition();  //low-going edge for start bit
	mSerialSimulationData.Advance( samples_per_bit );  //add start bit time

	U8 mask = 0x1 << 7;
	for( U32 i=0; i<8; i++ )
	{
		if( ( byte & mask ) != 0 )
			mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
		else
			mSerialSimulationData.TransitionIfNeeded( BIT_LOW );

		mSerialSimulationData.Advance( samples_per_bit );
		mask = mask >> 1;
	}

	mSerialSimulationData.TransitionIfNeeded( BIT_HIGH ); //we need to end high

	//lets pad the end a bit for the stop bit:
	mSerialSimulationData.Advance( samples_per_bit );
}
