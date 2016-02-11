#include "DynamixelAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "DynamixelAnalyzer.h"
#include "DynamixelAnalyzerSettings.h"
#include <iostream>
#include <fstream>

DynamixelAnalyzerResults::DynamixelAnalyzerResults( DynamixelAnalyzer* analyzer, DynamixelAnalyzerSettings* settings )
:	AnalyzerResults(),
	mSettings( settings ),
	mAnalyzer( analyzer )
{
}

DynamixelAnalyzerResults::~DynamixelAnalyzerResults()
{
}

void DynamixelAnalyzerResults::GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base )
{
	ClearResultStrings();
	Frame frame = GetFrame( frame_index );

	char number_str[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );
	char packet_checksum[8];
	AnalyzerHelpers::GetNumberString( ( frame.mData2 >> ( 1 * 8 ) ) & 0xff, display_base, 8, packet_checksum, 8 );
	char packet_length[8];
	AnalyzerHelpers::GetNumberString( ( frame.mData2 >> ( 2 * 8 ) ) & 0xff, display_base, 8, packet_length, 8 );

	char packet_type = frame.mData2 & 0xff;
	//char checksum = ( frame.mData2 >> ( 1 * 8 ) ) & 0xff;

	if ( packet_type == DynamixelAnalyzer::NONE )
	{
		AddResultString( "RP" );
		AddResultString( "REPLY" );
		AddResultString( "REPLY ID(", number_str , ")" );
		AddResultString( "REPLY ID(", number_str , ") LEN(", packet_length, ")" );
		AddResultString( "REPLY ID(", number_str , ") LEN(", packet_length, ") CHKSUM: ", packet_checksum );
	}
	else if ( packet_type == DynamixelAnalyzer::APING )
	{
		AddResultString( "P" );
		AddResultString( "PING" );
		AddResultString( "PING ID(", number_str , ")" );
	}
	else if ( packet_type == DynamixelAnalyzer::READ )
	{
		AddResultString( "R" );
		AddResultString( "READ" );
		AddResultString( "READ ID(", number_str , ")" );
		AddResultString( "READ ID(", number_str , ") LEN(", packet_length, ")" );
		AddResultString( "READ ID(", number_str , ") LEN(", packet_length, ") CHKSUM: ", packet_checksum );
	}
	else if ( packet_type == DynamixelAnalyzer::WRITE )
	{
		AddResultString( "W" );
		AddResultString( "WRITE" );
		AddResultString( "WRITE ID(", number_str , ")" );
		AddResultString( "WRITE ID(", number_str , ") LEN(", packet_length, ")" );
		AddResultString( "WRITE ID(", number_str , ") LEN(", packet_length, ") CHKSUM: ", packet_checksum );
	}
	else if ( packet_type == DynamixelAnalyzer::REG_WRITE )
	{
		AddResultString( "RW" );
		AddResultString( "REG_WRITE" );
		AddResultString( "REG_WRITE ID(", number_str , ")" );
		AddResultString( "REG_WRITE ID(", number_str , ") LEN(", packet_length, ")" );
		AddResultString( "REG_WRITEz ID(", number_str , ") LEN(", packet_length, ") CHKSUM: ", packet_checksum );
	}
	else if ( packet_type == DynamixelAnalyzer::ACTION )
	{
		AddResultString( "A" );
		AddResultString( "ACTION" );
		AddResultString( "ACTION ID(", number_str , ")" );		
	}
	else if ( packet_type == DynamixelAnalyzer::RESET )
	{
		AddResultString( "RS" );
		AddResultString( "RESET" );
		AddResultString( "RESET ID(", number_str , ")" );
		AddResultString( "RESET ID(", number_str , ") LEN(", packet_length, ")" );
	}
	else if ( packet_type == DynamixelAnalyzer::SYNC_WRITE )
	{
		AddResultString( "S" );
		AddResultString( "SYNC_WRITE" );
	}
	else
	{
		AddResultString( "?" );
		AddResultString( "unknown" );
		AddResultString( "unknown packet type" );
	}
}

void DynamixelAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id )
{
	std::ofstream file_stream( file, std::ios::out );

	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();

	file_stream << "Time [s],Value" << std::endl;

	U64 num_frames = GetNumFrames();
	for( U32 i=0; i < num_frames; i++ )
	{
		Frame frame = GetFrame( i );
		
		char time_str[128];
		AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128 );

		char number_str[128];
		AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );

		file_stream << time_str << "," << number_str << std::endl;

		if( UpdateExportProgressAndCheckForCancel( i, num_frames ) == true )
		{
			file_stream.close();
			return;
		}
	}

	file_stream.close();
}

void DynamixelAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
	Frame frame = GetFrame( frame_index );
	ClearResultStrings();

	char number_str[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, number_str, 128 );
	AddResultString( number_str );
}

void DynamixelAnalyzerResults::GeneratePacketTabularText( U64 packet_id, DisplayBase display_base )
{
	ClearResultStrings();
	AddResultString( "not supported" );
}

void DynamixelAnalyzerResults::GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base )
{
	ClearResultStrings();
	AddResultString( "not supported" );
}