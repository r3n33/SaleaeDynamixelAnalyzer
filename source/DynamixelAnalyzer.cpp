#include "DynamixelAnalyzer.h"
#include "DynamixelAnalyzerSettings.h"
#include <AnalyzerChannelData.h>

unsigned char reverse(unsigned char b)
{
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

DynamixelAnalyzer::DynamixelAnalyzer()
:	Analyzer(),  
	mSettings( new DynamixelAnalyzerSettings() ),
	mSimulationInitilized( false ),
	DecodeIndex( 0 )
{
	SetAnalyzerSettings( mSettings.get() );
}

DynamixelAnalyzer::~DynamixelAnalyzer()
{
	KillThread();
}

void DynamixelAnalyzer::WorkerThread()
{
	mResults.reset( new DynamixelAnalyzerResults( this, mSettings.get() ) );
	SetAnalyzerResults( mResults.get() );
	mResults->AddChannelBubblesWillAppearOn( mSettings->mInputChannel );

	mSampleRateHz = GetSampleRate();

	mSerial = GetAnalyzerChannelData( mSettings->mInputChannel );

	if( mSerial->GetBitState() == BIT_LOW )
		mSerial->AdvanceToNextEdge();

	U32 samples_per_bit = mSampleRateHz / mSettings->mBitRate;
	U32 samples_to_first_center_of_first_current_byte_bit = U32( 1.5 * double( mSampleRateHz ) / double( mSettings->mBitRate ) );

	U64 starting_sample;
	U64 data_samples_starting[256];		// Hold starting positions for all possible data byte positions. 

	for( ; ; )
	{
		U8 current_byte = 0;
		U8 mask = 1 << 7;
		
		mSerial->AdvanceToNextEdge(); //falling edge -- beginning of the start bit

		//U64 starting_sample = mSerial->GetSampleNumber();
		if ( DecodeIndex == DE_HEADER1 )
		{
			starting_sample = mSerial->GetSampleNumber();
		} 
		else if (DecodeIndex == DE_DATA)
		{
			data_samples_starting[mCount] = mSerial->GetSampleNumber();
		}

		mSerial->Advance( samples_to_first_center_of_first_current_byte_bit );

		for( U32 i=0; i<8; i++ )
		{
			//let's put a dot exactly where we sample this bit:
			//NOTE: Dot, ErrorDot, Square, ErrorSquare, UpArrow, DownArrow, X, ErrorX, Start, Stop, One, Zero
			//mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Start, mSettings->mInputChannel );

			if( mSerial->GetBitState() == BIT_HIGH )
				current_byte |= mask;

			mSerial->Advance( samples_per_bit );

			mask = mask >> 1;
		}

		//TODO: Inverting bits here because I cannot yet find how to add Inverstion to Settings
		current_byte = reverse( current_byte );

		//Process new byte
		
		switch ( DecodeIndex )
		{
			case DE_HEADER1:
				if ( current_byte == 0xFF )
				{
					DecodeIndex = DE_HEADER2;
					//starting_sample = mSerial->GetSampleNumber();
					mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Dot, mSettings->mInputChannel );
				}
				else
				{
					mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Stop, mSettings->mInputChannel );
				}
			break;
			case DE_HEADER2:
				if ( current_byte == 0xFF )
				{
					DecodeIndex = DE_ID;
					mChecksum = 1;
					mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Dot, mSettings->mInputChannel );
				}
				else
				{
					DecodeIndex = DE_HEADER1;
					mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorSquare, mSettings->mInputChannel );
				}
			break;
			case DE_ID:
				if ( current_byte != 0xFF )    // we are not allowed 3 0xff's in a row, ie. id != 0xff
				{   
					mID = current_byte;
					DecodeIndex = DE_LENGTH;
					mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::One, mSettings->mInputChannel );
				}
				else
				{
					DecodeIndex = DE_HEADER1;
					mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorX, mSettings->mInputChannel );
				}
			break;
			case DE_LENGTH:
				mLength = current_byte;
				DecodeIndex = DE_INSTRUCTION;
				mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Zero, mSettings->mInputChannel );
			break;
			case DE_INSTRUCTION:
				mInstruction = current_byte;
				mCount = 0;
				DecodeIndex = DE_DATA;
				mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::DownArrow, mSettings->mInputChannel );
				if ( mLength == 2 ) DecodeIndex = DE_CHECKSUM;
			break;
			case DE_DATA:
				mData[ mCount++ ] = current_byte;
				mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::UpArrow, mSettings->mInputChannel );
				if ( mCount >= mLength - 2 )
				{
					DecodeIndex = DE_CHECKSUM;
				}
			break;
			case DE_CHECKSUM:
				//We have a new frame to save! 
				Frame frame;
				frame.mFlags = 0;

				DecodeIndex = DE_HEADER1;
				if (  ( ~mChecksum & 0xff ) == ( current_byte & 0xff ) ) 
				{
					// Checksums match!
					mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Dot, mSettings->mInputChannel );
				}
				else
				{
					mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::ErrorDot, mSettings->mInputChannel );
					frame.mFlags = DISPLAY_AS_ERROR_FLAG;
				}

				//
				// Lets build our Frame. Right now all are done same way, except lets try to special case SYNC_WRITE, as we won't likely fit all 
				// of the data into one frame... So break out each servos part as their own frame 
				frame.mType = mInstruction;		// Save the packet type in mType
				frame.mData1 = mID | (mChecksum << (1 * 8)) | (mLength << (2 * 8)) | (mData[0] << (3 * 8)); // encode id and length and checksum + 5 data bytes

				// BUGBUG:: did two step as << 40 appears to error out...
				U64 mTemp =  (mData[1] << (0 * 8)) | (mData[2] << (1 * 8)) | (mData[3] << (2 * 8)) | (mData[4] << (3 * 8));
				frame.mData1 |= (mTemp << (4 * 8));

				// Use mData2 to store up to 8 bytes of the packet data. 
				frame.mData2 = (mData[5] << (0 * 8)) | (mData[6] << (1 * 8)) | (mData[7] << (2 * 8)) | (mData[8] << (3 * 8));
				mTemp = (mData[9] << (0 * 8)) | (mData[10] << (1 * 8)) | (mData[11] << (2 * 8)) | (mData[12] << (3 * 8));
				frame.mData2 |= (mTemp << (4 * 8));

				frame.mStartingSampleInclusive = starting_sample;

				// See if we are doing a SYNC_WRITE...
				if ((mInstruction == SYNC_WRITE) && (mLength > 4))
				{
					// Add Header Frame. 
					// Data byte: <start reg><reg count> 
					frame.mEndingSampleInclusive = data_samples_starting[1] + samples_per_bit * 10;

					mResults->AddFrame(frame);
					ReportProgress(frame.mEndingSampleInclusive);

					// Now lets figure out how many frames to add plus bytes per frame
					U8 count_of_servos = (mLength - 4) / (mData[1] + 1);	// Should validate this is correct but will try this for now...
					frame.mType = SYNC_WRITE_SERVO_DATA;
					U8 data_index = 2;
					for (U8 iServo = 0; iServo < count_of_servos; iServo++)
					{
						frame.mStartingSampleInclusive = data_samples_starting[data_index];
						// Now to encode the data bytes. 
						// mData1 - Maybe Servo ID, Starting index, count bytes
						// mData2 - Up to 8 bytes per servo... Could pack more... but
						// BUGBUG Should verify that count of bytes <= 8
						frame.mData1 = mData[data_index] | (mData[0] << 8) | (mData[1] << 16);
						frame.mData2 = 0;
						for (U8 i = data_index + mData[1]; i > mData[1]; i--)
							frame.mData2 = (frame.mData2 << 8) | mData[i];

						data_index += mData[1] + 1;	// advance to start of next one...

						// Now try to report this one. 
						if ((iServo+1) < count_of_servos)
							frame.mEndingSampleInclusive = data_samples_starting[data_index-1] + samples_per_bit * 10;
						else
							frame.mEndingSampleInclusive = mSerial->GetSampleNumber();

						mResults->AddFrame(frame);
						ReportProgress(frame.mEndingSampleInclusive);
					}

				}
				else
				{
					// Normal frames...
					frame.mEndingSampleInclusive = mSerial->GetSampleNumber();
					mResults->AddFrame(frame);
					ReportProgress(frame.mEndingSampleInclusive);
				}
			break;
		}
		mChecksum += current_byte;
		
		mResults->CommitResults();
	}
}

bool DynamixelAnalyzer::NeedsRerun()
{
	return false;
}

U32 DynamixelAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels )
{
	if( mSimulationInitilized == false )
	{
		mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), mSettings.get() );
		mSimulationInitilized = true;
	}

	return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 DynamixelAnalyzer::GetMinimumSampleRateHz()
{
	return mSettings->mBitRate * 4;
}

const char* DynamixelAnalyzer::GetAnalyzerName() const
{
	return "Dynamixel Protocol";
}

const char* GetAnalyzerName()
{
	return "Dynamixel Protocol";
}

Analyzer* CreateAnalyzer()
{
	return new DynamixelAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
	delete analyzer;
}