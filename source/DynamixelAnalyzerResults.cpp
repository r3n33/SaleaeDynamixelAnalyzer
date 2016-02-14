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

void DynamixelAnalyzerResults::GenerateBubbleText(U64 frame_index, Channel& channel, DisplayBase display_base)
{
	ClearResultStrings();
	Frame frame = GetFrame(frame_index);
	bool Package_Handled = false;

	// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
	// frame.mType has our = packet type
	// mData1 bytes low to high ID, Checksum, length, data0-data4
	// mData2  data5-12

	char id_str[128];
	AnalyzerHelpers::GetNumberString(frame.mData1 & 0xff, display_base, 8, id_str, 128);
	char packet_checksum[8];
	U8 checksum = ~((frame.mData1 >> (1 * 8)) & 0xff) & 0xff;
	AnalyzerHelpers::GetNumberString(checksum, display_base, 8, packet_checksum, 8);

	char Packet_length_string[8];
	U8 Packet_length = (frame.mData1 >> (2 * 8)) & 0xff;
	AnalyzerHelpers::GetNumberString(Packet_length, display_base, 8, Packet_length_string, 8);

	U8 packet_type = frame.mType;
	//char checksum = ( frame.mData2 >> ( 1 * 8 ) ) & 0xff;

	if ((packet_type == DynamixelAnalyzer::APING) && (Packet_length == 2))
	{
		AddResultString("P");
		AddResultString("PING");
		AddResultString("PING ID(", id_str, ")");
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::READ) && (Packet_length == 4))
	{
		char reg_start[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (3 * 8)) & 0xff, display_base, 8, reg_start, 8);
		char reg_count[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, 8);

		AddResultString("R");
		AddResultString("READ");
		AddResultString("RD(", id_str, ")");
		AddResultString("RD(", id_str, ") REG:", reg_start);
		AddResultString("RD(", id_str, ") REG:", reg_start, " LEN:", reg_count);
		AddResultString("READ(", id_str, ") REG:", reg_start, " LEN:", reg_count);
		//		AddResultString( "READ(", id_str , ") REG:", reg_start, " LEN:", reg_count, " CHKSUM: ", packet_checksum );
		Package_Handled = true;
	}
	else if ( ((packet_type == DynamixelAnalyzer::WRITE) || (packet_type == DynamixelAnalyzer::REG_WRITE)) && (Packet_length > 3))

	{
		// Assume packet must have at least two data bytes: starting register and at least one byte.
		char reg_start[8];
		char reg_count[8];
		U8 count_data_bytes = Packet_length - 3;
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (3 * 8)) & 0xff, display_base, 8, reg_start, 8);
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count, 8);

		char short_command_str[5];
		char long_command_str[12];
			if (packet_type == DynamixelAnalyzer::WRITE)
		{
			AddResultString("W");
			AddResultString("WRITE");
			strcpy_s(short_command_str, sizeof(short_command_str), "WR(");
			strcpy_s(long_command_str, sizeof(long_command_str), "WRITE(");
		}
		else
		{
			AddResultString("RW");
			AddResultString("REG_WRITE");
			strcpy_s(short_command_str, sizeof(short_command_str), "RW(");
			strcpy_s(long_command_str, sizeof(long_command_str), "REG WRITE(");
		}

		AddResultString( short_command_str, id_str , ")" );
		AddResultString( short_command_str, id_str, ") REG:", reg_start);
		AddResultString( short_command_str, id_str, ") REG:", reg_start, " LEN:", reg_count);
		// Try to build string showing bytes
		if (count_data_bytes > 0)
		{
			char w_str[8];
			U64 shift_data = frame.mData1 >> (3 * 8);
			// for more bytes lets try concatenating strings... 
			char remaining_data[120];
			sprintf_s(remaining_data, sizeof(remaining_data), ") LEN: %s D: ", reg_count);

			if (count_data_bytes > 13)
				count_data_bytes = 13;	// Max bytes we can store
			for (U8 i = 0; i < count_data_bytes; i++)
			{
				if (i == 2)
					AddResultString(short_command_str, id_str, ") REG:", reg_start, remaining_data);
				if (i == 5)
				{
					AddResultString(short_command_str, id_str, ") REG:", reg_start, remaining_data);
					shift_data = frame.mData2;
				}
				else
					shift_data >>= 8;

				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	// reuse string; 
				if (i != 0)
					strcat_s(remaining_data, sizeof(remaining_data), ", ");
				strcat_s(remaining_data, sizeof(remaining_data), w_str);
			}
			AddResultString(short_command_str, id_str, ") REG:", reg_start, remaining_data);
			AddResultString(long_command_str, id_str, ") REG:", reg_start, remaining_data);
		}
		else
			AddResultString(long_command_str, id_str, ") REG:", reg_start, " LEN:", reg_count);

		Package_Handled = true;
	}
	else if ( (packet_type == DynamixelAnalyzer::ACTION ) && (Packet_length == 2) )
	{
		AddResultString( "A" );
		AddResultString( "ACTION" );
		AddResultString( "ACTION ID(", id_str , ")" );		
		Package_Handled = true;
	}
	else if ( (packet_type == DynamixelAnalyzer::RESET ) && (Packet_length == 2) )
	{
		AddResultString( "RS" );
		AddResultString( "RESET" );
		AddResultString( "RESET ID(", id_str , ")" );
		AddResultString( "RESET ID(", id_str , ") LEN(", Packet_length_string, ")" );
		Package_Handled = true;
	}
	else if (( packet_type == DynamixelAnalyzer::SYNC_WRITE ) && (Packet_length >= 4))
	{
		char reg_start[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (3 * 8)) & 0xff, display_base, 8, reg_start, 8);
		char reg_count[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, 8);

		AddResultString("SW");
		AddResultString("SYNC_WRITE");
		AddResultString("SW(", id_str, ")");
		AddResultString("SW(", id_str, ") REG:", reg_start);
		AddResultString("SW(", id_str, ") REG:", reg_start, " LEN:", reg_count);
		char sync_write_string[50];
		sprintf_s(sync_write_string, sizeof(sync_write_string), ") REG:%s Len:%s", reg_start, reg_count);
		AddResultString("SYNC_WRITE(", id_str, sync_write_string);
		AddResultString("SYNC_WRITE(", id_str, sync_write_string, " CS:", packet_checksum);

		Package_Handled = true;
	}

	// If the package was not handled, and packet type 0 then probably normal response, else maybe respone with error status
	if (!Package_Handled)
	{
		char command_str[40];
		char reg_count[8];
		U8 count_data_bytes = Packet_length - 2;
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count, 8);

		if (packet_type == DynamixelAnalyzer::NONE)
		{
			AddResultString("RP");
			AddResultString("REPLY");
			AddResultString("RP(", id_str, ")");
			sprintf_s(command_str, sizeof(command_str), "RP(%s) LEN: %s", id_str, reg_count);
		}
		else
		{
			char status_str[8];
			AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, status_str, 8);
			AddResultString("SR");
			AddResultString("STATUS");
			AddResultString("SR:", status_str, "(", id_str, ")");
			sprintf_s(command_str, sizeof(command_str), "SR:%s(%s) LEN: %s", status_str, id_str, reg_count);
		}

		AddResultString(command_str);

		// Try to build string showing bytes
		if (count_data_bytes)
		{
			char w_str[8];
			char params_str[100];
			U64 shift_data = frame.mData1 >> (3 * 8);
			AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);
			sprintf_s(params_str, sizeof(params_str), " D: %s", w_str);
			AddResultString(command_str, params_str);

			if (count_data_bytes > 1)
			{
				// for more bytes lets try concatenating strings... 
				// we can reuse some of the above strings, as params_str has our first two parameters. 
				char remaining_data[100] = { 0 };
				if (count_data_bytes > 13)
					count_data_bytes = 13;	// Max bytes we can store
				for (U8 i = 1; i < count_data_bytes; i++)
				{
					if (i == 5)
					{
						AddResultString(command_str, params_str, remaining_data);  // Add partial so depends on how large space
						shift_data = frame.mData2;
					}
					else
						shift_data >>= 8;
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	// reuse string; 
					strcat_s(remaining_data, sizeof(remaining_data), ", ");
					strcat_s(remaining_data, sizeof(remaining_data), w_str);
				}
				AddResultString(command_str, params_str, remaining_data);  // Add full one. 

				//AddResultString("REPLY(", id_str, params_str, remaining_data);  // Add full one. 
			}
		}

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

		char id_str[128];
		AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, id_str, 128 );

		file_stream << time_str << "," << id_str << std::endl;

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

	char id_str[128];
	AnalyzerHelpers::GetNumberString( frame.mData1, display_base, 8, id_str, 128 );
	AddResultString( id_str );
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