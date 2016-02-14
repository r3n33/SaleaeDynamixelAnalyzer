#include "DynamixelSimulationDataGenerator.h"
#include "DynamixelAnalyzerSettings.h"

#include <AnalyzerHelpers.h>

// Add some delays into generated test 
static const char s_dxl_packets[] = { 	
	// First byte is count... 
	0x8, 0xff, 0xff, 0x01, 0x04, 0x02, 0x2b, 0x01, 0xcc, //Read temp of ID1
	0x8, 0xff, 0xff, 0xfe, 0x04, 0x03, 0x03, 0x01, 0xf6, //Write ID1 to broadcast
	0x6, 0xff, 0xff, 0x01, 0x02, 0x01, 0xFB,				//Ping ID1
	0x6, 0xff, 0xff, 0x02, 0x02, 0x06, 0xf7,				//Reset ID2 bad checksum
	0x9, 0xff, 0xff, 0x01, 0x05, 0x03, 0x1e, 0x90, 0x01, 0x47, // Move 1 to 400
	0x6, 0xff, 0xff, 0x01, 0x02, 0x00, 0xfc,				// reply OK
	0x2f,0xff, 0xff, 0xfe, 0x2b, 0x83, 0x18, 0x01, 0x08, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x07, 0x00, 0x0d,
		 0x00, 0x13, 0x00, 0x0a, 0x00, 0x10, 0x00, 0x04, 0x00, 0x09, 0x00, 0x0f, 0x00, 0x03, 0x00, 0x0c, 
	     0x00, 0x12, 0x00, 0x06, 0x00, 0x0b, 0x00, 0x11, 0x00, 0x05, 0x00, 0x14, 0x00, 0x15, 0x54, 
	0x0				 };									// End of list	
DynamixelSimulationDataGenerator::DynamixelSimulationDataGenerator()
:	mSerialText( s_dxl_packets ),
	mStringIndex( 0 ), mPacketIndex(0)
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

	if (mPacketIndex == 0)
	{
		mPacketIndex = s_dxl_packets[mStringIndex];
		mStringIndex++;
		if (mPacketIndex == 0)
		{
			// Finished the list
			mPacketIndex = s_dxl_packets[0];
			mStringIndex = 1;
		}
		mSerialSimulationData.Advance(samples_per_bit * 100);  //Put a gap in between packets
	}

	char byte = s_dxl_packets[ mStringIndex ];
	mStringIndex++;
	mPacketIndex--;

	//we're currenty high
	//let's move forward a little
	mSerialSimulationData.Advance( samples_per_bit * 10 );

	mSerialSimulationData.Transition();  //low-going edge for start bit
	mSerialSimulationData.Advance( samples_per_bit );  //add start bit time

	// lsb first...
	U8 mask = 0x1;
	for( U32 i=0; i<8; i++ )
	{
		if( ( byte & mask ) != 0 )
			mSerialSimulationData.TransitionIfNeeded( BIT_HIGH );
		else
			mSerialSimulationData.TransitionIfNeeded( BIT_LOW );

		mSerialSimulationData.Advance( samples_per_bit );
		mask = mask << 1;
	}

	mSerialSimulationData.TransitionIfNeeded( BIT_HIGH ); //we need to end high

	//lets pad the end a bit for the stop bit:
	mSerialSimulationData.Advance( samples_per_bit );
}
