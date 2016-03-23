#include "DynamixelAnalyzer.h"
#include "DynamixelAnalyzerSettings.h"
#include <AnalyzerChannelData.h>
#include <AnalyzerHelpers.h>

DynamixelAnalyzer::DynamixelAnalyzer()
:	Analyzer2(),  
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

void DynamixelAnalyzer::ComputeSampleOffsets()
{
	ClockGenerator clock_generator;
	clock_generator.Init(mSettings->mBitRate, mSampleRateHz);

	mSampleOffsets.clear();

	U32 num_bits = 8;

	mSampleOffsets.push_back(clock_generator.AdvanceByHalfPeriod(1.5));  //point to the center of the 1st bit (past the start bit)
	num_bits--;  //we just added the first bit.

	for (U32 i = 0; i<num_bits; i++)
	{
		mSampleOffsets.push_back(clock_generator.AdvanceByHalfPeriod());
	}

	//to check for framing errors, we also want to check
	//1/2 bit after the beginning of the stop bit
	mStartOfStopBitOffset = clock_generator.AdvanceByHalfPeriod(1.0);  //i.e. moving from the center of the last data bit (where we left off) to 1/2 period into the stop bit
}


void DynamixelAnalyzer::SetupResults()
{
	mResults.reset(new DynamixelAnalyzerResults(this, mSettings.get()));
	SetAnalyzerResults(mResults.get());
	mResults->AddChannelBubblesWillAppearOn(mSettings->mInputChannel);
}


void DynamixelAnalyzer::WorkerThread()
{
	mSampleRateHz = GetSampleRate();
	ComputeSampleOffsets();

	mSerial = GetAnalyzerChannelData( mSettings->mInputChannel );

	if( mSerial->GetBitState() == BIT_LOW )
		mSerial->AdvanceToNextEdge();

	U32 samples_per_bit = mSampleRateHz / mSettings->mBitRate;
	U32 samples_to_first_center_of_first_current_byte_bit = U32( 1.5 * double( mSampleRateHz ) / double( mSettings->mBitRate ) );

	U64 starting_sample;
	U64 data_samples_starting[256];		// Hold starting positions for all possible data byte positions. 

	U8 previous_ID = 0xff;
	U8 previous_instruction = 0xff;
	U8 previous_reg_start = 0xff;

	for( ; ; )
	{
		U8 current_byte = 0;
		U8 mask = 1 << 0;
		

		// Lets verify we have the right state for a start bit. 
		// before we continue. and like wise don't say it is a byte unless it also has the right stop bit...
		do {

			do {
				mSerial->AdvanceToNextEdge(); //falling edge -- beginning of the start bit

			} while ((mSerial->GetBitState() == BIT_HIGH));		// start bit should be logicall low. 

			//U64 starting_sample = mSerial->GetSampleNumber();
			if (DecodeIndex == DE_HEADER1)
			{
				starting_sample = mSerial->GetSampleNumber();
			}
			else
			{
				// Try checking for packets that are taking too long. 
				U64 packet_time_ms = (mSerial->GetSampleNumber() - starting_sample) / (mSampleRateHz / 1000);
				if (packet_time_ms > PACKET_TIMEOUT_MS)
				{
					DecodeIndex = DE_HEADER1;
					starting_sample = mSerial->GetSampleNumber();
				}
				else if (DecodeIndex == DE_DATA)
				{
					// Test should not be needed, but to be safe
					if (mCount < (sizeof(data_samples_starting)/sizeof(data_samples_starting[0])))
						data_samples_starting[mCount] = mSerial->GetSampleNumber();
				}
			}

//			mSerial->Advance(samples_to_first_center_of_first_current_byte_bit);

			for (U32 i = 0; i < 8; i++)
			{
				//let's put a dot exactly where we sample this bit:
				//NOTE: Dot, ErrorDot, Square, ErrorSquare, UpArrow, DownArrow, X, ErrorX, Start, Stop, One, Zero
				//mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Start, mSettings->mInputChannel );
				mSerial->Advance(mSampleOffsets[i]);
				if (mSerial->GetBitState() == BIT_HIGH)
					current_byte |= mask;

//				mSerial->Advance(samples_per_bit);
				mask = mask << 1;
			}
			mSerial->Advance(mStartOfStopBitOffset);
		} while (mSerial->GetBitState() != BIT_HIGH);		// Stop bit should be logically high

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
				frame.mData1 = mID | (mChecksum << (1 * 8)) | (mLength << (2 * 8)) | (mData[0] << (3 * 8)) |  // encode id and length and checksum + 5 data bytes
						((U64)mData[1] << (4 * 8)) | ((U64)mData[2] << (5 * 8)) | ((U64)mData[3] << (6 * 8)) | ((U64)mData[4] << (7 * 8));

				// Use mData2 to store up to 8 bytes of the packet data. 
				if (frame.mFlags == 0)
				{
					// Try hack if Same ID as previous and Instruction before was read and this is a 0 response then 
					// Maybe encode starting register as mData[12]
					if (mInstruction == DynamixelAnalyzer::NONE)
					{
						if ((mID == previous_ID) && (previous_instruction == DynamixelAnalyzer::READ))
							mData[12] = previous_reg_start;
						else
							mData[12] = 0xff;
					}

					frame.mData2 = (mData[5] << (0 * 8)) | (mData[6] << (1 * 8)) | (mData[7] << (2 * 8)) | (mData[8] << (3 * 8)) |
						((U64)mData[9] << (4 * 8)) | ((U64)mData[10] << (5 * 8)) | ((U64)mData[11] << (6 * 8)) | ((U64)mData[12] << (7 * 8));
				}
				else
					frame.mData2 = current_byte;	// in error frame have mData2 with the checksum byte. 

				frame.mStartingSampleInclusive = starting_sample;

				// See if we are doing a SYNC_WRITE...  But not if error!
				if ((mInstruction == SYNC_WRITE) && (mLength > 4) && (frame.mFlags == 0))
				{
					// Add Header Frame. 
					// Data byte: <start reg><reg count> 
					frame.mEndingSampleInclusive = data_samples_starting[1] + samples_per_bit * 10;

					mResults->AddFrame(frame);
					ReportProgress(frame.mEndingSampleInclusive);

					// Now lets figure out how many frames to add plus bytes per frame
					U8 count_of_servos = (mLength - 4) / (mData[1] + 1);	// Should validate this is correct but will try this for now...
					if (mData[1] && (mData[1] <=8) && count_of_servos /*&& ((count_of_servos *(mData[1]+1)+4) == mLength)*/)
					{
						frame.mType = SYNC_WRITE_SERVO_DATA;
						U8 data_index = 2;
						for (U8 iServo = 0; iServo < count_of_servos; iServo++)
						{
							//frame.mStartingSampleInclusive = data_samples_starting[data_index];
							frame.mStartingSampleInclusive = frame.mEndingSampleInclusive + 1;
							// Now to encode the data bytes. 
							// mData1 - Maybe Servo ID, 0, 0, Starting index, count bytes < updated same as other packets, but 
							// mData2 - Up to 8 bytes per servo... Could pack more... but
							// BUGBUG Should verify that count of bytes <= 8
							frame.mData1 = mData[data_index] | (mData[0] << (3 * 8)) | ((U64)mData[1] << (4 * 8));
							frame.mData2 = 0;
							for (U8 i = mData[1]; i > 0; i--)
								frame.mData2 = (frame.mData2 << 8) | mData[data_index + i];

							data_index += mData[1] + 1;	// advance to start of next one...

							// Now try to report this one. 
							if ((iServo + 1) < count_of_servos)
								frame.mEndingSampleInclusive = data_samples_starting[data_index - 1] + samples_per_bit * 10;
							else
								frame.mEndingSampleInclusive = mSerial->GetSampleNumber();

							mResults->AddFrame(frame);
							ReportProgress(frame.mEndingSampleInclusive);
						}
					}

				}
				else
				{
					// Normal frames...
					frame.mEndingSampleInclusive = mSerial->GetSampleNumber();
					mResults->AddFrame(frame);
					ReportProgress(frame.mEndingSampleInclusive);
				}
				previous_ID = mID;
				previous_instruction = mInstruction;
				if (previous_instruction == DynamixelAnalyzer::READ)
					previous_reg_start = mData[0];	// 0 should be reg start...
				else
					previous_reg_start = 0xff;		// Not read...

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