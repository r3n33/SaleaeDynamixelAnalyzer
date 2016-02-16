#define _CRT_SECURE_NO_WARNINGS		// Make windows compiler shut up...
#include "DynamixelAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "DynamixelAnalyzer.h"
#include "DynamixelAnalyzerSettings.h"
#include <fstream>

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

static const char * s_instruction_names[] = {
	"REPLY", "PING", "READ", "WRITE", "REG_WRITE", "ACTION", "RESET",
	"SYNC_WRITE", "SW_DATA", "REPLY_STAT" };

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
			snprintf(remaining_data, sizeof(remaining_data), ") REG: %s(%s) LEN: %s", reg_start_str, 
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
			snprintf(short_command_str, sizeof(short_command_str), "WR(%s) REG:%s", id_str, reg_start_str);
			snprintf(long_command_str, sizeof(long_command_str), "WRITE(%s) REG:%s", id_str, reg_start_str);
		}
		else
		{
			AddResultString("RW");
			AddResultString("REG_WRITE");
			AddResultString("RW(", id_str, ")");
			snprintf(short_command_str, sizeof(short_command_str), "RW(%s) REG:%s", id_str, reg_start_str);
			snprintf(long_command_str, sizeof(long_command_str), "REG WRITE(%s) REG:%s", id_str, reg_start_str);
		}

		AddResultString( short_command_str );
		AddResultString( short_command_str, " LEN:", reg_count);
		// Try to build string showing bytes
		if (count_data_bytes > 0)
		{
			char w_str[8];
			U64 shift_data = frame.mData1 >> (3 * 8);
			// for more bytes lets try concatenating strings... 
			snprintf(remaining_data, sizeof(remaining_data), " LEN: %s D: ", reg_count);

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
					strncat(remaining_data, ", ", sizeof(remaining_data));
				strncat(remaining_data, w_str, sizeof(remaining_data));
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
			snprintf(cmd_header_str, sizeof(cmd_header_str), "");
		else
		{
			AddResultString("SW(", id_str, ")");
			snprintf(cmd_header_str, sizeof(cmd_header_str), "(%s)", id_str);
		}
		AddResultString("SW",cmd_header_str,  " REG:", reg_start_str);
		AddResultString("SW",cmd_header_str, " REG:", reg_start_str, " LEN:", reg_count);

		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			snprintf(remaining_data, sizeof(remaining_data), " REG: %s(%s) LEN: %s", reg_start_str,
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
		strcpy (remaining_data, ":");
		U8 count_data_bytes = (frame.mData1 >> (4 * 8)) & 0xff;
		for (U8 i = 0; i < count_data_bytes; i++)
		{
			if (i == 2)
				AddResultString(id_str,  remaining_data);
			shift_data >>= 8;

			AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);	
			if (i != 0)
				strncat(remaining_data, ", ", sizeof(remaining_data));
			strncat(remaining_data, w_str, sizeof(remaining_data));
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
			snprintf(command_str, sizeof(command_str), "RP(%s) LEN: %s", id_str, reg_count);
		}
		else
		{
			char status_str[8];
			AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, status_str, 8);
			AddResultString("SR");
			AddResultString("STATUS");
			AddResultString("SR:", status_str, "(", id_str, ")");
			snprintf(command_str, sizeof(command_str), "SR:%s(%s) LEN: %s", status_str, id_str, reg_count);
		}

		AddResultString(command_str);

		// Try to build string showing bytes
		if (count_data_bytes)
		{
			char w_str[8];
			char params_str[100];
			U64 shift_data = frame.mData1 >> (3 * 8);
			AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);
			snprintf(params_str, sizeof(params_str), " D: %s", w_str);
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
					strncat(remaining_data, ", ", sizeof(remaining_data));
					strncat(remaining_data, w_str, sizeof(remaining_data));
				}
				AddResultString(command_str, params_str, remaining_data);  // Add full one. 

				//AddResultString("REPLY(", id_str, params_str, remaining_data);  // Add full one. 
			}
		}

	}
}

void DynamixelAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id )
{
	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();
	char w_str[8];

	std::ofstream file_stream(file, std::ios::out);

	file_stream << "Time [s],Instruction, Instructions(S), ID, Errors, Reg start, Reg(s), count, data" << std::endl;

	U64 num_frames = GetNumFrames();
	for( U32 frame_index=0; frame_index < num_frames; frame_index++ )
	{
		char id_str[8];
		char reg_start_str[8];
		const char *reg_start_name_str_ptr;
		char reg_count_str[8];
		char time_str[16];
		char status_str[16];
		const char *instruct_str_ptr;

		Frame frame = GetFrame( frame_index );
		
		AnalyzerHelpers::GetTimeString( frame.mStartingSampleInclusive, trigger_sample, sample_rate, time_str, sizeof(time_str) );

		//-----------------------------------------------------------------------------
		bool Package_Handled = false;
		U8 Packet_length = (frame.mData1 >> (2 * 8)) & 0xff;


		// First lets try to output the common stuff, like time, type, id
	

		// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
		// frame.mType has our = packet type
		// mData1 bytes low to high ID, Checksum, length, data0-data4
		// mData2  data5-12

		U8 packet_type = frame.mType;

		AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, w_str, 8);
		AnalyzerHelpers::GetNumberString(frame.mData1 & 0xff, display_base, 8, id_str, 8);

		// Note: only used in some packets...
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		U8 reg_count = (frame.mData1 >> (4 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);

		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
			reg_start_name_str_ptr = s_ax_register_names[reg_start];
		else
			reg_start_name_str_ptr = "";


		AnalyzerHelpers::GetNumberString(reg_count, display_base, 8, reg_count_str, 8);

		// Setup initial 
		if (frame.mFlags & DISPLAY_AS_ERROR_FLAG)
			strncpy(status_str, "CHECKSUM", sizeof(status_str));
		else
			status_str[0] = 0;

		// Figure out our Instruction string
		if (packet_type < (sizeof(s_instruction_names) / sizeof(s_instruction_names[0])))
			instruct_str_ptr = s_instruction_names[packet_type];
		else if (packet_type == DynamixelAnalyzer::SYNC_WRITE)
			instruct_str_ptr = s_instruction_names[7];	// BUGBUG:: Should not hard code
		else if (packet_type == DynamixelAnalyzer::SYNC_WRITE_SERVO_DATA)
		{
			instruct_str_ptr = s_instruction_names[8];	// BUGBUG:: Should not hard code
			w_str[0] = 0;	// lets not output our coded 0xff
		}
		else
			instruct_str_ptr = "";

		file_stream << time_str << "," << w_str << "," << instruct_str_ptr << "," << id_str;

		// Now lets handle the different packet types
		if ((Packet_length == 2) && (
				(packet_type == DynamixelAnalyzer::ACTION) ||
				(packet_type == DynamixelAnalyzer::RESET) ||
				(packet_type == DynamixelAnalyzer::APING)))
		{
			if (strlen(status_str))
				file_stream << "," << status_str;
			Package_Handled = true;
		}
		else if ((packet_type == DynamixelAnalyzer::READ) && (Packet_length == 4))
		{
			file_stream << "," << status_str << "," << reg_start_str << "," << reg_start_name_str_ptr << "," << reg_count_str;
			Package_Handled = true;
		}

		else if (((packet_type == DynamixelAnalyzer::WRITE) || (packet_type == DynamixelAnalyzer::REG_WRITE)) && (Packet_length > 3))
		{
			U8 count_data_bytes = Packet_length - 3;
			// output first part... Warning: Write register count is not stored but derived...
			AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count_str, 8);
			file_stream << "," << status_str << "," << reg_start_str << "," << reg_start_name_str_ptr << "," << reg_count_str;

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
					file_stream << "," << w_str;
				}
			}
			Package_Handled = true;
		}
		else if ((packet_type == DynamixelAnalyzer::SYNC_WRITE) && (Packet_length >= 4))
		{
			file_stream << "," << status_str << "," << reg_start_str << "," << reg_start_name_str_ptr << "," << reg_count_str;
			Package_Handled = true;
		}
		else if (packet_type == DynamixelAnalyzer::SYNC_WRITE_SERVO_DATA)
		{
			file_stream << "," << status_str << "," << reg_start_str << "," << reg_start_name_str_ptr << "," << reg_count_str;
			U64 shift_data = frame.mData2;

			// for more bytes lets try concatenating strings... 
			U8 count_data_bytes = (frame.mData1 >> (4 * 8)) & 0xff;
			for (U8 i = 0; i < count_data_bytes; i++)
			{
				shift_data >>= 8;

				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);
				file_stream << "," << w_str;
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
				file_stream << "," << status_str << ",,," << reg_count_str;
			}
			else
			{
				AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, status_str, sizeof(status_str));
				file_stream << "," << status_str << ",,," << reg_count_str;
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
					file_stream << "," << w_str;
				}
			}
		}

		file_stream <<  std::endl;

		if (UpdateExportProgressAndCheckForCancel(frame_index, num_frames) == true)
			break;
	}

	UpdateExportProgressAndCheckForCancel(num_frames, num_frames);
	file_stream.close();

}

void DynamixelAnalyzerResults::GenerateFrameTabularText( U64 frame_index, DisplayBase display_base )
{
	ClearTabularText();

	char frame_text_str[128];
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
		snprintf(frame_text_str, sizeof(frame_text_str), "PG %s", id_str);
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::READ) && (Packet_length == 4))
	{
		char reg_start_str[8];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		char reg_count[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, 8);

		snprintf(frame_text_str, sizeof(frame_text_str), "RD %s: R:%s L:%s", id_str, reg_start_str, reg_count);
		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			strncat(frame_text_str, " - ", sizeof(frame_text_str));
			strncat(frame_text_str, s_ax_register_names[reg_start], sizeof(frame_text_str));
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
			snprintf(frame_text_str, sizeof(frame_text_str), "WR %s: R:%s L:%s ", id_str, reg_start_str, reg_count);
		}
		else
		{
			snprintf(frame_text_str, sizeof(frame_text_str), "RW %s: R:%s L:%s ", id_str, reg_start_str, reg_count);
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
					strncat(frame_text_str, ", ", sizeof(frame_text_str));
				strncat(frame_text_str, w_str, sizeof(frame_text_str));
				if (strlen(frame_text_str) >= (sizeof(frame_text_str) - 1))
					break;
			}
			if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
			{
				strncat(frame_text_str, " - ", sizeof(frame_text_str));
				strncat(frame_text_str, s_ax_register_names[reg_start], sizeof(frame_text_str));
			}
		}
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::ACTION) && (Packet_length == 2))
	{
		snprintf(frame_text_str, sizeof(frame_text_str), "AC %s", id_str);
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::RESET) && (Packet_length == 2))
	{
		snprintf(frame_text_str, sizeof(frame_text_str), "RS %s", id_str);
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::SYNC_WRITE) && (Packet_length >= 4))
	{
		char reg_start_str[8];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, 8);
		char reg_count[8];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, 8);

		snprintf(frame_text_str, sizeof(frame_text_str), "SW %s: R:%s L:%s", id_str, reg_start_str, reg_count);
		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			strncat(frame_text_str, " - ", sizeof(frame_text_str));
			strncat(frame_text_str, s_ax_register_names[reg_start], sizeof(frame_text_str));
		}
		Package_Handled = true;
	}
	else if (packet_type == DynamixelAnalyzer::SYNC_WRITE_SERVO_DATA)
	{
		snprintf(frame_text_str, sizeof(frame_text_str), "SD %s ", id_str);
		char w_str[8];

		U64 shift_data = frame.mData2;

		// for more bytes lets try concatenating strings... 
		U8 count_data_bytes = (frame.mData1 >> (4 * 8)) & 0xff;
		for (U8 i = 0; i < count_data_bytes; i++)
		{
			shift_data >>= 8;

			AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, 8);
			if (i != 0)
				strncat(frame_text_str, ", ", sizeof(frame_text_str));
			strncat(frame_text_str, w_str, sizeof(frame_text_str));
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
			snprintf(frame_text_str, sizeof(frame_text_str), "RP %s ", id_str);
		}
		else
		{
			char status_str[8];
			AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, status_str, 8);
			snprintf(frame_text_str, sizeof(frame_text_str), "SR %s %s ", id_str, status_str);
		}

		// Try to build string showing bytes
		if (count_data_bytes)
		{
			strncat(frame_text_str, "L:", sizeof(frame_text_str));
			strncat(frame_text_str, reg_count, sizeof(frame_text_str));
			strncat(frame_text_str, "D:", sizeof(frame_text_str));

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
					strncat(frame_text_str, ", ", sizeof(frame_text_str));
				strncat(frame_text_str, w_str, sizeof(frame_text_str));
			}
		}
	}
	AddTabularText(frame_text_str);
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