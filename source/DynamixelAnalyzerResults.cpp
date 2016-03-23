#define _CRT_SECURE_NO_WARNINGS		// Make windows compiler shut up...
#include "DynamixelAnalyzerResults.h"
#include <AnalyzerHelpers.h>
#include "DynamixelAnalyzer.h"
#include "DynamixelAnalyzerSettings.h"
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

static const char * s_instruction_names[] = {
	"REPLY", "PING", "READ", "WRITE", "REG_WRITE", "ACTION", "RESET",
	"SYNC_WRITE", "SW_DATA", "REPLY_STAT" };

static const U8 s_is_register_pair_start[] = {
	1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
	1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,
	1, 0 
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
	std::stringstream ss;

	// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
	// frame.mType has our = packet type
	// mData1 bytes low to high ID, Checksum, length, data0-data4
	// mData2  data5-12
	bool fShowWords = mSettings->mShowWords;	// see if we are setup to show words when appropriate. 
	char id_str[20];
	AnalyzerHelpers::GetNumberString(frame.mData1 & 0xff, display_base, 8, id_str, sizeof(id_str));
	char packet_checksum[20];
	U8 checksum = ~((frame.mData1 >> (1 * 8)) & 0xff) & 0xff;
	AnalyzerHelpers::GetNumberString(checksum, display_base, 8, packet_checksum, sizeof(packet_checksum));

	char Packet_length_string[20];
	U8 Packet_length = (frame.mData1 >> (2 * 8)) & 0xff;
	AnalyzerHelpers::GetNumberString(Packet_length, display_base, 8, Packet_length_string, sizeof(Packet_length_string));

	U8 packet_type = frame.mType;

	// Several packet types use the register start and Register length, so move that out here
	char reg_start_str[20];
	U16 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
	AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, sizeof(reg_start_str));
	char reg_count_str[20];
	AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count_str, sizeof(reg_count_str));

	if (frame.mFlags & DISPLAY_AS_ERROR_FLAG)
	{
		U8 checksum = ~( frame.mData1 >> ( 1 * 8 ) ) & 0xff;
		char checksum_str[10];
		char calc_checksum_str[10];
		AnalyzerHelpers::GetNumberString(checksum, display_base, 8, calc_checksum_str, sizeof(calc_checksum_str));
		AnalyzerHelpers::GetNumberString(frame.mData2 & 0xff, display_base, 8, checksum_str, sizeof(checksum_str));
		AddResultString("*");
		AddResultString("Checksum");
		AddResultString("CHK: ", checksum_str, "!=", calc_checksum_str);
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::APING) && (Packet_length == 2))
	{
		AddResultString("P");
		AddResultString("PING");
		AddResultString("PING ID(", id_str, ")");
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::READ) && (Packet_length == 4))
	{
		AddResultString("R");
		AddResultString("READ");
		AddResultString("RD(", id_str, ")");
		AddResultString("RD(", id_str, ") REG:", reg_start_str);
		AddResultString("RD(", id_str, ") REG:", reg_start_str, " LEN:", reg_count_str);
		AddResultString("READ(", id_str, ") REG:", reg_start_str, " LEN:", reg_count_str);
		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			ss << "RD(" << id_str << ") REG: " << reg_start_str << "(" << s_ax_register_names[reg_start]
				<< ") LEN: " << reg_count_str;
			AddResultString(ss.str().c_str());

		}
		//		AddResultString( "READ(", id_str , ") REG:", reg_start, " LEN:", reg_count_str, " CHKSUM: ", packet_checksum );
		Package_Handled = true;
	}
	else if ( ((packet_type == DynamixelAnalyzer::WRITE) || (packet_type == DynamixelAnalyzer::REG_WRITE)) && (Packet_length > 3))

	{
		// Assume packet must have at least two data bytes: starting register and at least one byte.
		U8 reg_start = (frame.mData1 >> (4 * 8)) & 0xff;
		U8 count_data_bytes = Packet_length - 3;

		// Need to redo as length is calculated. 
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count_str, sizeof(reg_count_str));

		char short_command_str[5];
		char long_command_str[10];
			if (packet_type == DynamixelAnalyzer::WRITE)
		{
			AddResultString("W");
			AddResultString("WRITE");
			AddResultString("WR(", id_str, ")");
			strcpy(short_command_str, "WR");
			strcpy(long_command_str, "WRITE");
		}
		else
		{
			AddResultString("RW");
			AddResultString("REG_WRITE");
			AddResultString("RW(", id_str, ")");
			strcpy(short_command_str, "RW");
			strcpy(long_command_str, "REG WRITE");
		}

		ss << "(" << id_str << ") REG:" << reg_start_str;

		AddResultString( short_command_str, ss.str().c_str());
		ss << " LEN: " << reg_count_str;
		AddResultString(short_command_str, ss.str().c_str());

		// Try to build string showing bytes
		if (count_data_bytes > 0)
		{
			ss << "D: ";
			char w_str[20];
			U64 shift_data = frame.mData1 >> (3 * 8);
			// for more bytes lets try concatenating strings... 

			if (count_data_bytes > 13)
				count_data_bytes = 13;	// Max bytes we can store
			for (U8 index_data_byte = 0; index_data_byte < count_data_bytes; )
			{
				if (index_data_byte == 2)
					AddResultString(short_command_str, ss.str().c_str());

				if (index_data_byte != 0)
					ss << ", ";

				// now see if register is associated with a pair of registers. 
				if (fShowWords && ((reg_start+index_data_byte) < sizeof(s_is_register_pair_start)) && (index_data_byte < (count_data_bytes - 1))
					&& s_is_register_pair_start[reg_start+index_data_byte])
				{
					// need to special case index 4 as it splits the two mdata members
					U16 wval;
					if (index_data_byte == 4) {
						wval = ((frame.mData2 & 0xff) << 8) | shift_data & 0xff;
						shift_data = frame.mData2 >> 8;		// setup for next one
					}
					else {
						wval = shift_data & 0xffff;
						shift_data >>= 8;
					}
					AnalyzerHelpers::GetNumberString(wval, display_base, 16, w_str, sizeof(w_str));	// reuse string; 
					index_data_byte += 2;  // update index to next byte to process
					ss << "$";
				}
				else
				{
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
					index_data_byte++;
					if (index_data_byte == 4)
					{
						shift_data = frame.mData2;		// setup for next one
						AddResultString(short_command_str, ss.str().c_str());
					}
					else
						shift_data >>= 8;				}

				ss << w_str;
			}
			AddResultString(short_command_str, ss.str().c_str());
			AddResultString(long_command_str, ss.str().c_str());
			
			if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
			{
				AddResultString(short_command_str, "(", s_ax_register_names[reg_start], ")", ss.str().c_str());
				AddResultString(long_command_str, "(", s_ax_register_names[reg_start], ")", ss.str().c_str());
			}

		}
		else
			AddResultString(long_command_str, id_str, ") REG:", reg_start_str, " LEN:", reg_count_str);

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
//		char cmd_header_str[50];

		AddResultString("SW");
		AddResultString("SYNC_WRITE");
		// if ID == 0xfe no need to say it... 
		if ((frame.mData1 & 0xff) == 0xfe)
			ss << " ";
		else
		{
			ss << "(" << id_str << ") ";
			AddResultString(ss.str().c_str());
		}

		ss << "REG:" << reg_start_str;
		AddResultString("SW", ss.str().c_str());
		ss << " LEN:" << reg_count_str;
		AddResultString("SW", ss.str().c_str());

		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			ss << " REG: " << reg_start_str << "(" << s_ax_register_names[reg_start]
				<< ") LEN: " << reg_count_str;
			AddResultString("SW", ss.str().c_str());
			AddResultString("SYNC_WRITE", ss.str().c_str());

		}

		Package_Handled = true;
	}
	else if (packet_type == DynamixelAnalyzer::SYNC_WRITE_SERVO_DATA)
	{
		AddResultString("D");	
		AddResultString(id_str);	// Show Servo number.
		char w_str[20];

		U64 shift_data = frame.mData2;

		// for more bytes lets try concatenating strings... 
		ss << ":";
		U8 count_data_bytes = (frame.mData1 >> (4 * 8)) & 0xff;
		for (U8 index_data_byte = 0; index_data_byte < count_data_bytes;)
		{
			if (index_data_byte == 2)
				AddResultString(id_str, ss.str().c_str());
			if (index_data_byte != 0)
				ss << ", ";

			if (fShowWords && ((reg_start+index_data_byte) < sizeof(s_is_register_pair_start)) && (index_data_byte < (count_data_bytes - 1))
				&& s_is_register_pair_start[reg_start + index_data_byte])
			{
				AnalyzerHelpers::GetNumberString(shift_data & 0xffff, display_base, 16, w_str, sizeof(w_str));	// reuse string; 
				shift_data >>= 16;
				index_data_byte += 2;
				ss << "$";
			}
			else
			{
				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
				shift_data >>= 8;
				index_data_byte++;
			}
			ss << w_str;
		}
		AddResultString(id_str, ss.str().c_str());
		Package_Handled = true;
	}

	// If the package was not handled, and packet type 0 then probably normal response, else maybe respone with error status
	if (!Package_Handled)
	{
//		char command_str[40];
		U8 count_data_bytes = Packet_length - 2;
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count_str, sizeof(reg_count_str));

		if (packet_type == DynamixelAnalyzer::NONE)
		{
			AddResultString("RP");
			AddResultString("REPLY");
			AddResultString("RP(", id_str, ")");
			ss << "RP(" << id_str << ") LEN: " << reg_count_str;
			reg_start = (frame.mData2 >> (7 * 8)) & 0xff;
		}
		else
		{
			char status_str[20];
			AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, status_str, sizeof(status_str));
			AddResultString("SR");
			AddResultString("STATUS");
			AddResultString("SR:", status_str, "(", id_str, ")");
			ss << "SR:" << status_str << "(" << id_str << ") LEN: " << reg_count_str;
			reg_start = 0xff;		// make sure 
		}

		AddResultString(ss.str().c_str());

		// Try to build string showing bytes
		if (count_data_bytes)
		{
			char w_str[20];
			U64 shift_data = frame.mData1 >> (3 * 8);
			ss << " D: ";

			if (count_data_bytes > 12)
				count_data_bytes = 12;	// Max bytes we can store
			for (U8 index_data_byte = 0; index_data_byte < count_data_bytes; )
			{
				if (index_data_byte != 0)
					ss << ", ";

				// now see if register is associated with a pair of registers. 
				if (fShowWords && ((reg_start + index_data_byte) < sizeof(s_is_register_pair_start)) && (index_data_byte < (count_data_bytes - 1))
					&& s_is_register_pair_start[reg_start + index_data_byte])
				{
					// need to special case index 4 as it splits the two mdata members
					U16 wval;
					if (index_data_byte == 4) {
						wval = ((frame.mData2 & 0xff) << 8) | shift_data & 0xff;
						shift_data = frame.mData2 >> 8;		// setup for next one
					}
					else {
						wval = shift_data & 0xffff;
						shift_data >>= 16;
					}
					AnalyzerHelpers::GetNumberString(wval, display_base, 16, w_str, sizeof(w_str));	// reuse string; 
					index_data_byte += 2;  // update index to next byte to process
					ss << "$";
				}
				else
				{
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
					index_data_byte++;
					if (index_data_byte == 3)
					{
						shift_data = frame.mData2;
					}
					else
						shift_data >>= 8;
				}

				ss << w_str;
				AddResultString(ss.str().c_str());  // Add up to this point...  
			}


			//AddResultString("REPLY(", id_str, params_str, remaining_data);  // Add full one. 
		}

	}
}

void DynamixelAnalyzerResults::GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id )
{
	U64 trigger_sample = mAnalyzer->GetTriggerSample();
	U32 sample_rate = mAnalyzer->GetSampleRate();
	char w_str[20];

	std::ofstream file_stream(file, std::ios::out);

	file_stream << "Time [s],Instruction, Instructions(S), ID, Errors, Reg start, Reg(s), count, data" << std::endl;

	U64 num_frames = GetNumFrames();
	for( U32 frame_index=0; frame_index < num_frames; frame_index++ )
	{
		char id_str[20];
		char reg_start_str[20];
		const char *reg_start_name_str_ptr;
		char reg_count_str[20];
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

		AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, w_str, sizeof(w_str));
		AnalyzerHelpers::GetNumberString(frame.mData1 & 0xff, display_base, 8, id_str, sizeof(id_str));

		// Note: only used in some packets...
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		U8 reg_count = (frame.mData1 >> (4 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, sizeof(reg_start_str));

		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
			reg_start_name_str_ptr = s_ax_register_names[reg_start];
		else
			reg_start_name_str_ptr = "";


		AnalyzerHelpers::GetNumberString(reg_count, display_base, 8, reg_count_str, sizeof(reg_count_str));

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
			instruct_str_ptr = s_instruction_names[20];	// BUGBUG:: Should not hard code
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
			AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count_str, sizeof(reg_count_str));
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
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
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

				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));
				file_stream << "," << w_str;
			}
			Package_Handled = true;
		}

		// If the package was not handled, and packet type 0 then probably normal response, else maybe respone with error status
		if (!Package_Handled)
		{
			U8 count_data_bytes = Packet_length - 2;
			AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count_str, sizeof(reg_count_str));

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
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
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
	std::stringstream ss;
	Frame frame = GetFrame(frame_index);
	bool Package_Handled = false;
	U8 Packet_length = (frame.mData1 >> (2 * 8)) & 0xff;
	bool fShowWords = mSettings->mShowWords;	// see if we are setup to show words when appropriate. 


	// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
	// frame.mType has our = packet type
	// mData1 bytes low to high ID, Checksum, length, data0-data4
	// mData2  data5-12

	char id_str[16];
	AnalyzerHelpers::GetNumberString(frame.mData1 & 0xff, display_base, 8, id_str, sizeof(id_str));

	U8 packet_type = frame.mType;
	//char checksum = ( frame.mData2 >> ( 1 * 8 ) ) & 0xff;

	if (frame.mFlags & DISPLAY_AS_ERROR_FLAG)
	{
		U8 checksum = ~(frame.mData1 >> (1 * 8)) & 0xff;
		char checksum_str[10];
		char calc_checksum_str[10];
		AnalyzerHelpers::GetNumberString(checksum, display_base, 8, calc_checksum_str, sizeof(calc_checksum_str));
		AnalyzerHelpers::GetNumberString(frame.mData2 & 0xff, display_base, 8, checksum_str, sizeof(checksum_str));
		AddResultString("*");
		AddResultString("Checksum");

		ss << "CHK " << checksum_str << " != " << calc_checksum_str;
		AddResultString(ss.str().c_str());
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::APING) && (Packet_length == 2))
	{
		ss << "PG " << id_str;
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::READ) && (Packet_length == 4))
	{
		char reg_start_str[16];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, sizeof(reg_start_str));
		char reg_count[20];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, sizeof(reg_count));

		ss << "RD " << id_str << ": R:" << reg_start_str << " L:" << reg_count;
		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			ss <<" - ";
			ss << s_ax_register_names[reg_start];
		}
		Package_Handled = true;
	}

	else if (((packet_type == DynamixelAnalyzer::WRITE) || (packet_type == DynamixelAnalyzer::REG_WRITE)) && (Packet_length > 3))
	{
		// Assume packet must have at least two data bytes: starting register and at least one byte.
		char reg_count[20];
		char reg_start_str[20];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		U8 count_data_bytes = Packet_length - 3;

		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, sizeof(reg_start_str));
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count, sizeof(reg_count));

		if (packet_type == DynamixelAnalyzer::WRITE)
		{
			ss << "WR " << id_str << ": R:" << reg_start_str << " L:" << reg_count;
		}
		else
		{
			ss << "RW " << id_str << ": R:" << reg_start_str << " L:" << reg_count;
		}

		// Try to build string showing bytes
		if (count_data_bytes > 0)
		{
			U64 shift_data = frame.mData1 >> (3 * 8);
			char w_str[20];
			if (count_data_bytes > 13)
				count_data_bytes = 13;	// Max bytes we can store
			for (U8 index_data_byte = 0; index_data_byte < count_data_bytes; )
			{
				if (index_data_byte != 0)
					ss << ", ";

				// now see if register is associated with a pair of registers. 
				if (fShowWords && ((reg_start + index_data_byte) < sizeof(s_is_register_pair_start)) && (index_data_byte < (count_data_bytes - 1))
					&& s_is_register_pair_start[reg_start + index_data_byte])
				{
					// need to special case index 4 as it splits the two mdata members
					U16 wval;
					if (index_data_byte == 4) {
						wval = ((frame.mData2 & 0xff) << 8) | shift_data & 0xff;
						shift_data = frame.mData2 >> 8;		// setup for next one
					}
					else {
						wval = shift_data & 0xffff;
						shift_data >>= 8;
					}
					AnalyzerHelpers::GetNumberString(wval, display_base, 16, w_str, sizeof(w_str));	// reuse string; 
					index_data_byte += 2;  // update index to next byte to process
					ss << "$";
				}
				else
				{
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
					index_data_byte++;
					if (index_data_byte == 4)
						shift_data = frame.mData2;
					else
						shift_data >>= 8;
				}

				ss << w_str;
			}

			if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
			{
				ss << " - " << s_ax_register_names[reg_start];
			}
		}
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::ACTION) && (Packet_length == 2))
	{
		ss << "AC " << id_str;
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::RESET) && (Packet_length == 2))
	{
		ss << "RS " << id_str;
		Package_Handled = true;
	}
	else if ((packet_type == DynamixelAnalyzer::SYNC_WRITE) && (Packet_length >= 4))
	{
		char reg_start_str[20];
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, sizeof(reg_start_str));
		char reg_count[20];
		AnalyzerHelpers::GetNumberString((frame.mData1 >> (4 * 8)) & 0xff, display_base, 8, reg_count, sizeof(reg_count));

		ss <<"SW " << id_str << ": R:" << reg_start_str << " L:" << reg_count;
		if (reg_start < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
		{
			ss << " - " << s_ax_register_names[reg_start];
		}
		Package_Handled = true;
	}
	else if (packet_type == DynamixelAnalyzer::SYNC_WRITE_SERVO_DATA)
	{
		ss << "SD " << id_str;
		char w_str[20];

		U64 shift_data = frame.mData2;
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;

		// for more bytes lets try concatenating strings... 
		U8 count_data_bytes = (frame.mData1 >> (4 * 8)) & 0xff;
		if (count_data_bytes > 7)
			count_data_bytes = 7;
		for (U8 index_data_byte = 0; index_data_byte < count_data_bytes;)
		{
			if (index_data_byte != 0)
				ss << ", ";

			if (fShowWords && ((reg_start + index_data_byte) < sizeof(s_is_register_pair_start)) && (index_data_byte < (count_data_bytes - 1))
				&& s_is_register_pair_start[(reg_start + index_data_byte)])
			{
				AnalyzerHelpers::GetNumberString(shift_data & 0xffff, display_base, 16, w_str, sizeof(w_str));	// reuse string; 
				shift_data >>= 16;
				index_data_byte += 2;
				ss << "$";
			}
			else
			{
				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
				shift_data >>= 8;
				index_data_byte++;
			}
			ss << w_str;
		}

		Package_Handled = true;
	}

	// If the package was not handled, and packet type 0 then probably normal response, else maybe respone with error status
	if (!Package_Handled)
	{
		char reg_count[16];
		U8 count_data_bytes = Packet_length - 2;
		AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count, sizeof(reg_count));

		if (packet_type == DynamixelAnalyzer::NONE)
		{
			ss << "RP " << id_str;
		}
		else
		{
			char status_str[16];
			AnalyzerHelpers::GetNumberString(packet_type, display_base, 8, status_str, sizeof(status_str));
			ss << "SR " << id_str << " " << status_str;
		}

		// Try to build string showing bytes
		if (count_data_bytes)
		{
			ss << "L:" << reg_count << "D:";

			char w_str[16];
			U64 shift_data = frame.mData1 >> (2 * 8);
			// for more bytes lets try concatenating strings... 
			// we can reuse some of the above strings, as params_str has our first two parameters. 
			// BUGBUG:: try limiting to 5
			if (count_data_bytes > 5)
				count_data_bytes = 5;	// Max bytes we can store
			for (U8 i = 0; i < count_data_bytes; i++)
			{
				if (i == 5)
					shift_data = frame.mData2;
				else
					shift_data >>= 8;
				AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
				if (i != 0)
					ss <<", ";
				ss << w_str;
			}
		}
	}
	AddTabularText(ss.str().c_str());
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


