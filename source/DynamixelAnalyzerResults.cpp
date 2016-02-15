#include "DynamixelAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "DynamixelAnalyzer.h"
#include "DynamixelAnalyzerSettings.h"
#include <iostream>
#include <fstream>
#include <sstream>

//=============================================================================
// Define Global/Static data
//=============================================================================
static const char * s_ax_register_names[] = {
	"MODEL", "MODEL_H", "VER","ID","BAUD","DELAY","CWL","CWL_H",
	"CCWL","CCWL_H","DATA2","LTEMP","LVOLTD","LVOLTU","MTORQUE", "MTORQUE_H",
	"RLEVEL","ALED","ASHUT","OMODE","DCAL","DCAL_H","UCAL","UCAL_H",
	/** RAM AREA **/
	"TENABLE","LED","CWMAR","CCWMAR","CWSLOPE","CCWSLOPE","GOAL","GOAL_H",
	"GSPEED","GSPEED_H","TLIMIT","TLIMIT_H","PPOS","PPOS_H","PSPEED","PSPEED_H",
	"PLOAD","PLOAD_H","PVOLT","PTEMP","RINST","PTIME","MOVING","LOCK",
	"PUNCH","PUNCH_H"
};


//=============================================================================
// class DynamixelAnalyzerResults 
//=============================================================================

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
	char remaining_data[120];

	// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
	// frame.mType has our = packet type
	// mData1 bytes low to high ID, Checksum, length, data0-data4
	// mData2  data5-12

	char id_str[8];
	AnalyzerHelpers::GetNumberString(frame.mData1 & 0xff, display_base, 8, id_str, 8);
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
		char reg_start_str[8];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		char reg_count[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, 8);

		AddResultString("R");
		AddResultString("READ");
		AddResultString("RD(", id_str, ")");
		AddResultString("RD(", id_str, ") REG:", reg_start_str);
		AddResultString("RD(", id_str, ") REG:", reg_start_str, " LEN:", reg_count);
		AddResultString("READ(", id_str, ") REG:", reg_start_str, " LEN:", reg_count);
		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			sprintf_s(remaining_data, sizeof(remaining_data), ") REG: %s(%s) LEN: %s", reg_start_str, 
				s_ax_register_names[reg_start], reg_count);
			AddResultString("RD(", id_str, remaining_data);

		}
		//		AddResultString( "READ(", id_str , ") REG:", reg_start, " LEN:", reg_count, " CHKSUM: ", packet_checksum );
		Package_Handled = true;
	}
	else if ( ((packet_type == DynamixelAnalyzer::WRITE) || (packet_type == DynamixelAnalyzer::REG_WRITE)) && (Packet_length > 3))

	{
		// Assume packet must have at least two data bytes: starting register and at least one byte.
		char reg_count[8];
		char reg_start_str[8];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		U8 count_data_bytes = Packet_length - 3;

		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count, 8);

		char short_command_str[50];
		char long_command_str[30];
			if (packet_type == DynamixelAnalyzer::WRITE)
		{
			AddResultString("W");
			AddResultString("WRITE");
			AddResultString("WR(", id_str, ")");
			sprintf_s(short_command_str, sizeof(short_command_str), "WR(%s) REG:%s", id_str, reg_start_str);
			sprintf_s(long_command_str, sizeof(long_command_str), "WRITE(%s) REG:%s", id_str, reg_start_str);
		}
		else
		{
			AddResultString("RW");
			AddResultString("REG_WRITE");
			AddResultString("RW(", id_str, ")");
			sprintf_s(short_command_str, sizeof(short_command_str), "RW(%s) REG:%s", id_str, reg_start_str);
			sprintf_s(long_command_str, sizeof(long_command_str), "REG WRITE(%s) REG:%s", id_str, reg_start_str);
		}

		AddResultString( short_command_str );
		AddResultString( short_command_str, " LEN:", reg_count);
		// Try to build string showing bytes
		if (count_data_bytes > 0)
		{
			char w_str[8];
			U64 shift_data = frame.mData1 >> (3 * 8);
			// for more bytes lets try concatenating strings... 
			sprintf_s(remaining_data, sizeof(remaining_data), " LEN: %s D: ", reg_count);

			if (count_data_bytes > 13)
				count_data_bytes = 13;	// Max bytes we can store
			for (U8 i = 0; i < count_data_bytes; i++)
			{
				if (i == 2)
					AddResultString(short_command_str, remaining_data);
				if (i == 5)
				{
					AddResultString(short_command_str, remaining_data);
					shift_data = frame.mData2;
				}
				else
					shift_data >>= 8;

				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	// reuse string; 
				if (i != 0)
					strcat_s(remaining_data, sizeof(remaining_data), ", ");
				strcat_s(remaining_data, sizeof(remaining_data), w_str);
			}
			AddResultString(short_command_str, remaining_data);
			AddResultString(long_command_str,  remaining_data);
			
			if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
			{
				AddResultString(short_command_str, "(", s_ax_register_names[reg_start], ")",remaining_data);
				AddResultString(long_command_str, "(", s_ax_register_names[reg_start], ")", remaining_data);
			}

		}
		else
			AddResultString(long_command_str, id_str, ") REG:", reg_start_str, " LEN:", reg_count);

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
		char cmd_header_str[50];
		char reg_start_str[8];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		char reg_count[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, 8);

		AddResultString("SW");
		AddResultString("SYNC_WRITE");
		// if ID == 0xfe no need to say it... 
		if ((frame.mData1 & 0xff) == 0xfe)
			sprintf_s(cmd_header_str, sizeof(cmd_header_str), "");
		else
		{
			AddResultString("SW(", id_str, ")");
			sprintf_s(cmd_header_str, sizeof(cmd_header_str), "(%s)", id_str);
		}
		AddResultString("SW",cmd_header_str,  " REG:", reg_start_str);
		AddResultString("SW",cmd_header_str, " REG:", reg_start_str, " LEN:", reg_count);

		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			sprintf_s(remaining_data, sizeof(remaining_data), " REG: %s(%s) LEN: %s", reg_start_str,
				s_ax_register_names[reg_start], reg_count);
			AddResultString("SW",cmd_header_str, remaining_data);
			AddResultString("SYNC_WRITE", cmd_header_str, remaining_data);

		}

		Package_Handled = true;
	}
	else if (packet_type == DynamixelAnalyzer::SYNC_WRITE_SERVO_DATA)
	{
		AddResultString("D");	
		AddResultString(id_str);	// Show Servo number.
		char w_str[8];

		U64 shift_data = frame.mData2;

		// for more bytes lets try concatenating strings... 
		strcpy_s (remaining_data, sizeof(remaining_data), ":");
		U8 count_data_bytes = (frame.mData1 >> (4 * 8)) & 0xff;
		for (U8 i = 0; i < count_data_bytes; i++)
		{
			if (i == 2)
				AddResultString(id_str,  remaining_data);
			shift_data >>= 8;

			AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	
			if (i != 0)
				strcat_s(remaining_data, sizeof(remaining_data), ", ");
			strcat_s(remaining_data, sizeof(remaining_data), w_str);
		}
		AddResultString(id_str, remaining_data);
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
				remaining_data[0] =  0;
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
	std::stringstream ss;


	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();
	char w_str[8];

	void* f = AnalyzerHelpers::StartFile(file);

	ss << "Time [s],Type, ID, Errors, Reg start, count, data" << std::endl;

	U64 num_frames = GetNumFrames();
	for( U32 frame_index=0; frame_index < num_frames; frame_index++ )
	{
		Frame frame = GetFrame( frame_index );
		
		char time_str[128];
		AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, 128 );

		//-----------------------------------------------------------------------------
		bool Package_Handled = false;
		char remaining_data[120];
		U8 Packet_length = (frame.mData1 >> (2 * 8)) & 0xff;


		// First lets try to output the common stuff, like time, type, id
	

		// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
		// frame.mType has our = packet type
		// mData1 bytes low to high ID, Checksum, length, data0-data4
		// mData2  data5-12

		char id_str[8];
		char reg_start_str[8];
		char reg_count_str[8];
		U8 packet_type = frame.mType;

		AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, w_str, 8);
		AnalyzerHelpers::GetNumberString(frame.mData1 & 0xff, display_base, 8, id_str, 8);

		// Note: only used in some packets...
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		U8 reg_count = (frame.mData1 >> (4 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		AnalyzerHelpers::GetNumberString(reg_count, display_base, 8, reg_count_str, 8);


		ss << time_str << "," << w_str << "," << id_str;

		// Now lets handle the different packet types
		if ((Packet_length == 2) && (
				(packet_type == DynamixelAnalyzer::ACTION) ||
				(packet_type == DynamixelAnalyzer::RESET) ||
				(packet_type == DynamixelAnalyzer::APING)))
		{
			Package_Handled = true;
		}
		else if ((packet_type == DynamixelAnalyzer::READ) && (Packet_length == 4))
		{
			ss << ",," << reg_start_str << "," << reg_count_str;
			Package_Handled = true;
		}

		else if (((packet_type == DynamixelAnalyzer::WRITE) || (packet_type == DynamixelAnalyzer::REG_WRITE)) && (Packet_length > 3))
		{
			U8 count_data_bytes = Packet_length - 3;
			// output first part... Warning: Write register count is not stored but derived...
			AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count_str, 8);
			ss << ",," << reg_start_str << "," << reg_count_str;

			// Try to build string showing bytes
			if (count_data_bytes > 0)
			{
				U64 shift_data = frame.mData1 >> (3 * 8);
				if (count_data_bytes > 13)
					count_data_bytes = 13;	// Max bytes we can store
				for (U8 i = 0; i < count_data_bytes; i++)
				{
					if (i == 5)
						shift_data = frame.mData2;
					else
						shift_data >>= 8;
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	// reuse string; 
					ss << "," << w_str;
				}
			}
			Package_Handled = true;
		}
		else if ((packet_type == DynamixelAnalyzer::SYNC_WRITE) && (Packet_length >= 4))
		{
			ss << ",," << reg_start_str << "," << reg_count_str;
			Package_Handled = true;
		}
		else if (packet_type == DynamixelAnalyzer::SYNC_WRITE_SERVO_DATA)
		{
			ss << ",," << reg_start_str << "," << reg_count_str;
			U64 shift_data = frame.mData2;

			// for more bytes lets try concatenating strings... 
			U8 count_data_bytes = (frame.mData1 >> (2 * 8)) & 0xff;
			for (U8 i = 0; i < count_data_bytes; i++)
			{
				shift_data >>= 8;

				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);
				ss << "," << w_str;
			}
			Package_Handled = true;
		}

		// If the package was not handled, and packet type 0 then probably normal response, else maybe respone with error status
		if (!Package_Handled)
		{
			U8 count_data_bytes = Packet_length - 2;
			AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count_str, 8);

			if (packet_type == DynamixelAnalyzer::NONE)
			{
				ss << ",,," << reg_count_str;
			}
			else
			{
				char status_str[8];
				AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, status_str, 8);
				ss << "," << status_str << ",," << reg_count_str;
			}

			// Try to build string showing bytes
			if (count_data_bytes)
			{
				U64 shift_data = frame.mData1 >> (2 * 8);

				if (count_data_bytes > 13)
					count_data_bytes = 13;	// Max bytes we can store
				for (U8 i = 0; i < count_data_bytes; i++)
				{
					if (i == 5)
						shift_data = frame.mData2;
					else
						shift_data >>= 8;
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	// reuse string; 
					ss << "," << w_str;
				}
			}
		}

		ss <<  std::endl;

		AnalyzerHelpers::AppendToFile((U8*)ss.str().c_str(), ss.str().length(), f);
		ss.str(std::string());
		if( UpdateExportProgressAndCheckForCancel( frame_index, num_frames ) == true )
		{
			AnalyzerHelpers::EndFile(f);
			return;
		}
	}

	UpdateExportProgressAndCheckForCancel(num_frames, num_frames);
	AnalyzerHelpers::EndFile(f);

}

bool DynamixelAnalyzerResults::GenerateFrameText(U64 frame_index, DisplayBase display_base, char* string_ptr, U16 string_len)
{
	Frame frame = GetFrame(frame_index);
	bool Package_Handled = false;
	char remaining_data[120];
	U8 Packet_length = (frame.mData1 >> (2 * 8)) & 0xff;


	// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
	// frame.mType has our = packet type
	// mData1 bytes low to high ID, Checksum, length, data0-data4
	// mData2  data5-12

	char id_str[8];
	AnalyzerHelpers::GetNumberString(frame.mData1 & 0xff, display_base, 8, id_str, 8);

	U8 packet_type = frame.mType;
	//char checksum = ( frame.mData2 >> ( 1 * 8 ) ) & 0xff;

	if ((packet_type == DynamixelAnalyzer::APING) && (Packet_length == 2))
	{
		sprintf_s(string_ptr, string_len, "PG %s", id_str);
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::READ) && (Packet_length == 4))
	{
		char reg_start_str[8];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		char reg_count[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, 8);

		sprintf_s(string_ptr, string_len, "RD %s: R:%s L:%s", id_str, reg_start_str, reg_count);
		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			strcat_s(string_ptr, string_len, " - ");
			strcat_s(string_ptr, string_len, s_ax_register_names[reg_start]);
		}
		Package_Handled = true;
	}

	else if (((packet_type == DynamixelAnalyzer::WRITE) || (packet_type == DynamixelAnalyzer::REG_WRITE)) && (Packet_length > 3))
	{
		// Assume packet must have at least two data bytes: starting register and at least one byte.
		char reg_count[8];
		char reg_start_str[8];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		U8 count_data_bytes = Packet_length - 3;

		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count, 8);

		if (packet_type == DynamixelAnalyzer::WRITE)
		{
			sprintf_s(string_ptr, string_len, "WR %s: R:%s L:%s ", id_str, reg_start_str, reg_count);
		}
		else
		{
			sprintf_s(string_ptr, string_len, "RW %s: R:%s L:%s ", id_str, reg_start_str, reg_count);
		}

		// Try to build string showing bytes
		if (count_data_bytes > 0)
		{
			U64 shift_data = frame.mData1 >> (3 * 8);
			char w_str[8];
			if (count_data_bytes > 13)
				count_data_bytes = 13;	// Max bytes we can store
			for (U8 i = 0; i < count_data_bytes; i++)
			{
				if (i == 5)
					shift_data = frame.mData2;
				else
					shift_data >>= 8;
				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	// reuse string; 

				if (i != 0)
					strcat_s(string_ptr, string_len, ", ");
				strcat_s(string_ptr, string_len, w_str);
				if (strlen(string_ptr) >= (string_len - 1))
					break;
			}
			if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
			{
				strcat_s(string_ptr, string_len, " - ");
				strcat_s(string_ptr, string_len, s_ax_register_names[reg_start]);
			}
		}
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::ACTION) && (Packet_length == 2))
	{
		sprintf_s(string_ptr, string_len, "AC %s", id_str);
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::RESET) && (Packet_length == 2))
	{
		sprintf_s(string_ptr, string_len, "RS %s", id_str);
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::SYNC_WRITE) && (Packet_length >= 4))
	{
		char reg_start_str[8];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		char reg_count[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, 8);

		sprintf_s(string_ptr, string_len, "SW %s: R:%s L:%s", id_str, reg_start_str, reg_count);
		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			strcat_s(string_ptr, string_len, " - ");
			strcat_s(string_ptr, string_len, s_ax_register_names[reg_start]);
		}
		Package_Handled = true;
	}
	else if (packet_type == DynamixelAnalyzer::SYNC_WRITE_SERVO_DATA)
	{
		sprintf_s(string_ptr, string_len, "SD %s ", id_str);
		char w_str[8];

		U64 shift_data = frame.mData2;

		// for more bytes lets try concatenating strings... 
		U8 count_data_bytes = (frame.mData1 >> (4 * 8)) & 0xff;
		for (U8 i = 0; i < count_data_bytes; i++)
		{
			shift_data >>= 8;

			AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);
			if (i != 0)
				strcat_s(string_ptr, string_len, ", ");
			strcat_s(string_ptr, string_len, w_str);
		}
		Package_Handled = true;
	}

	// If the package was not handled, and packet type 0 then probably normal response, else maybe respone with error status
	if (!Package_Handled)
	{
		char reg_count[8];
		U8 count_data_bytes = Packet_length - 2;
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count, 8);

		if (packet_type == DynamixelAnalyzer::NONE)
		{
			sprintf_s(string_ptr, string_len, "RP %s ", id_str);
		}
		else
		{
			char status_str[8];
			AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, status_str, 8);
			sprintf_s(string_ptr, string_len, "SR %s %s ", id_str, status_str);
		}

		// Try to build string showing bytes
		if (count_data_bytes)
		{
			strcat_s(string_ptr, string_len, "L:");
			strcat_s(string_ptr, string_len, reg_count);
			strcat_s(string_ptr, string_len, "D:");

			char w_str[8];
			U64 shift_data = frame.mData1 >> (2 * 8);

			// for more bytes lets try concatenating strings... 
			// we can reuse some of the above strings, as params_str has our first two parameters. 
			remaining_data[0] = 0;
			if (count_data_bytes > 13)
				count_data_bytes = 13;	// Max bytes we can store
			for (U8 i = 0; i < count_data_bytes; i++)
			{
				if (i == 5)
					shift_data = frame.mData2;
				else
					shift_data >>= 8;
				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	// reuse string; 
				if (i != 0)
					strcat_s(string_ptr, string_len, ", ");
				strcat_s(string_ptr, string_len, w_str);
			}
		}
	}
	return true;
}

void DynamixelAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
	ClearTabularText();

	char id_str[128];
	if (GenerateFrameText(frame_index, display_base, id_str, sizeof(id_str)))
	{
		AddTabularText(id_str);
	}
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