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
static const char * s_instruction_names[] = {
	"REPLY", "PING", "READ", "WRITE", "REG_WRITE", "ACTION", "RESET",
	"SYNC_WRITE", "SW_DATA", "REPLY_STAT" };


// AX Servos
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

static const U8 s_is_ax_register_pair_start[] = {
	1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0,
	1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,
	1, 0 
};

// MX Servos
static const char * s_mx_register_names[] = {
	"MODEL", "MODEL_H", "VER","ID","BAUD","DELAY","CWL","CWL_H",
	"CCWL","CCWL_H","?","LTEMP","LVOLTD","LVOLTU","MTORQUE", "MTORQUE_H",
	"RLEVEL","ALED","ASHUT","?","MTOFSET","MTOFSET_L","RESD","?",
	/** RAM AREA **/
	"TENABLE","LED","DGAIN","IGAIN","PGAIN","?","GOAL","GOAL_H",
	"GSPEED","GSPEED_H","TLIMIT","TLIMIT_H","PPOS","PPOS_H","PSPEED","PSPEED_H",
	"PLOAD","PLOAD_H","PVOLT","PTEMP","RINST","?","MOVING","LOCK",
	//0x30
	"PUNCH","PUNCH_H","?","?","?","?","?","?",
	"?","?","?","?","?","?","?","?",
	//0x40
	"?","?","?","?","CURR", "CURR_H", "TCME", "GTORQ",
	"QTORQ_H","GACCEL"
};

static const U8 s_is_mx_register_pair_start[] = {
	1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0,
	0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
	1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 1, 0, 0, 1, 0, 0
};

// XL Servos
static const char * s_xl_register_names[] = {
	"MODEL", "MODEL_H", "VER","ID","BAUD","DELAY","CWL","CWL_H",
	"CCWL","CCWL_H","?","CMODE", "LTEMP","LVOLTD","LVOLTU",	"MTORQUE", 
	"MTORQUE_H", "RLEVEL", "ASHUT","?","?","?","?","?",
	/** RAM AREA **/
	"TENABLE","LED","DGAIN","IGAIN","PGAIN","?","GOAL","GOAL_H",
	"GSPEED","GSPEED_H","?", "TLIMIT","TLIMIT_H","PPOS","PPOS_H","PSPEED",
	"PSPEED_H", "PLOAD","PLOAD_H","?", "?","PVOLT", "PTEMP","RINST",
	//0x30
	"?","MOVING","HSTAT", "PUNCH","PUNCH_H"
};

static const U8 s_is_xl_register_pair_start[] = {
	1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
	1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0,
	0, 0, 0, 1, 
};

//USB2AX - probably not needed as does not forward messages on normal AX BUSS

//CM730(ish) controllers. 
static const char * s_cm730_register_names[] = {
	"MODEL", "MODEL_H", "VER","ID","BAUD","DELAY","","",
	"","","","","LVOLTD","LVOLTU","", "",
	"RLEVEL","","","","","","","",
	/** RAM AREA **/
	"POWER", "LPANNEL", "LHEAD", "LHEAD_H", "LEYE", "LEYE_H", "BUTTON", "",
	"D1", "D2", "D3","D4","D5","D6", "GYROZ", "GYROZ_H",
	"GYROY","GYROY_H","GYROX","GYROX_H", "ACCX", "ACCX_H","ACCY", "ACCY_H",
	"ACCZ","ACCZ_H", "ADC0","ADC1","ADC1_H", "ADC2","ADC2_H", "ADC3",
	"ADC3_H","ADC4","ADC4_H","ADC5","ADC5_H","ADC6","ADC6_H", "ADC7",
	"ADC7_H","ADC8","ADC8_H","ADC9","ADC9_H","ADC10","ADC10_H", "ADC11",
	"ADC11_H","ADC12","ADC12_H","ADC13","ADC13_H","ADC14","ADC14_H", "ADC15",
	"ADC15_H"
};

static const U8 s_is_cm730_register_pair_start[] = {
	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
	1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
	0, 1, 0, 1, 0, 1, 0, 1, 0
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
	std::ostringstream ss;
	const char *pregister_name;

	// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
	// frame.mType has our = packet type
	// mData1 bytes low to high ID, Checksum, length, data0-data4
	// mData2  data5-12
	char id_str[20];
	U8 servo_id = frame.mData1 & 0xff;
	AnalyzerHelpers::GetNumberString(servo_id, display_base, 8, id_str, sizeof(id_str));
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
		ss << "RD(" << id_str << ") REG: " << reg_start_str;
		AddResultString(ss.str().c_str());
		
		if ((pregister_name = GetServoRegisterName(servo_id, reg_start)))
		{
			ss << "(" << pregister_name << ")";
			AddResultString(ss.str().c_str());
		}

		ss << " LEN:" << reg_count_str;
		AddResultString(ss.str().c_str());

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
				if ((index_data_byte < (count_data_bytes - 1)) && IsServoRegisterStartOfPair(servo_id, reg_start + index_data_byte))
				{
					// need to special case index 4 as it splits the two mdata members
					U32 wval;
					if (index_data_byte == 4) {
						wval = ((U16)(frame.mData2 & 0xff) << 8) | (shift_data & 0xff);
						shift_data = frame.mData2 >> 8;		// setup for next one
					}
					else {
						wval = (shift_data & 0xffff);
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
			
			if ((pregister_name = GetServoRegisterName(servo_id, reg_start)))
			{
				AddResultString(short_command_str, "(", pregister_name, ")", ss.str().c_str());
				AddResultString(long_command_str, "(", pregister_name, ")", ss.str().c_str());
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

		if ((pregister_name = GetServoRegisterName(servo_id, reg_start)))
		{
			ss << " REG: " << reg_start_str << "(" << pregister_name
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

			if ((index_data_byte < (count_data_bytes - 1)) && IsServoRegisterStartOfPair(servo_id, reg_start + index_data_byte))
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
				if ((index_data_byte < (count_data_bytes - 1)) && IsServoRegisterStartOfPair(servo_id, reg_start + index_data_byte))
				{
					// need to special case index 4 as it splits the two mdata members
					U16 wval;
					if (index_data_byte == 4) {
						wval = ((frame.mData2 & 0xff) << 8) | (shift_data & 0xff);
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
	const char *pregister_name;

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
		U8 servo_id = frame.mData1 & 0xff;
		AnalyzerHelpers::GetNumberString(servo_id, display_base, 8, id_str, sizeof(id_str));

		// Note: only used in some packets...
		U8 reg_start = (frame.mData1 >> (3 * 8)) & 0xff;
		U8 reg_count = (frame.mData1 >> (4 * 8)) & 0xff;
		AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, sizeof(reg_start_str));

		if ((pregister_name = GetServoRegisterName(servo_id, reg_start)))
			reg_start_name_str_ptr = pregister_name;
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
			AnalyzerHelpers::GetNumberString(count_data_bytes, display_base, 8, reg_count_str, sizeof(reg_count_str));
			file_stream << "," << status_str << "," << reg_start_str << "," << reg_start_name_str_ptr << "," << reg_count_str;

			// Try to build string showing bytes
			if (count_data_bytes > 0)
			{
				U64 shift_data = frame.mData1 >> (3 * 8);
				if (count_data_bytes > 13)
					count_data_bytes = 13;	// Max bytes we can store
				for (U8 index_data_byte = 0; index_data_byte < count_data_bytes; )
				{
					// now see if register is associated with a pair of registers. 
					if ((index_data_byte < (count_data_bytes - 1)) && IsServoRegisterStartOfPair(servo_id, reg_start + index_data_byte))
					{
						// need to special case index 4 as it splits the two mdata members
						U16 wval;
						if (index_data_byte == 4) {
							wval = ((frame.mData2 & 0xff) << 8) | (shift_data & 0xff);
							shift_data = frame.mData2 >> 8;		// setup for next one
						}
						else {
							wval = shift_data & 0xffff;
							shift_data >>= 8;
						}
						AnalyzerHelpers::GetNumberString(wval, display_base, 16, w_str, sizeof(w_str));	// reuse string; 
						index_data_byte += 2;  // update index to next byte to process
						file_stream << "$";
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

					file_stream << w_str;
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
			for (U8 index_data_byte = 0; index_data_byte < count_data_bytes;)
			{
				file_stream << ",";
				if ((index_data_byte < (count_data_bytes - 1)) && IsServoRegisterStartOfPair(servo_id, reg_start + index_data_byte))
				{
					AnalyzerHelpers::GetNumberString(shift_data & 0xffff, display_base, 16, w_str, sizeof(w_str));	// reuse string; 
					shift_data >>= 16;
					index_data_byte += 2;
					file_stream << "$";
				}
				else
				{
					AnalyzerHelpers::GetNumberString(shift_data & 0xff, display_base, 8, w_str, sizeof(w_str));	// reuse string; 
					shift_data >>= 8;
					index_data_byte++;
				}
				file_stream << w_str;
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
				reg_start = (frame.mData2 >> (7 * 8)) & 0xff;
				if (reg_start != 0xff)
				{
					AnalyzerHelpers::GetNumberString(reg_start, display_base, 8, reg_start_str, sizeof(reg_start_str));
					if ((pregister_name = GetServoRegisterName(servo_id, reg_start)))
						reg_start_name_str_ptr = pregister_name;
					else
						reg_start_name_str_ptr = "";

					file_stream << "," << status_str << "," << reg_start_str << ","<< reg_start_name_str_ptr << "," << reg_count_str;
				}
				else
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
				U64 shift_data = frame.mData1 >> (3 * 8);

				if (count_data_bytes > 12)
					count_data_bytes = 12;	// Max bytes we can store

				for (U8 index_data_byte = 0; index_data_byte < count_data_bytes; )
				{
					file_stream << ", ";

					// now see if register is associated with a pair of registers. 
					if ((index_data_byte < (count_data_bytes - 1)) && IsServoRegisterStartOfPair(servo_id, reg_start + index_data_byte))
					{
						// need to special case index 4 as it splits the two mdata members
						U16 wval;
						if (index_data_byte == 4) {
							wval = ((frame.mData2 & 0xff) << 8) | (shift_data & 0xff);
							shift_data = frame.mData2 >> 8;		// setup for next one
						}
						else {
							wval = shift_data & 0xffff;
							shift_data >>= 16;
						}
						AnalyzerHelpers::GetNumberString(wval, display_base, 16, w_str, sizeof(w_str));	// reuse string; 
						index_data_byte += 2;  // update index to next byte to process
						file_stream << "$";
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
					file_stream << w_str;
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
	std::ostringstream ss;
	Frame frame = GetFrame(frame_index);
	bool Package_Handled = false;
	U8 Packet_length = (frame.mData1 >> (2 * 8)) & 0xff;
	const char *pregister_name;


	// Note: the mData1 and mData2 are encoded with as much of the data as we can fit.
	// frame.mType has our = packet type
	// mData1 bytes low to high ID, Checksum, length, data0-data4
	// mData2  data5-12

	char id_str[16];
	U8 servo_id = frame.mData1 & 0xff;
	AnalyzerHelpers::GetNumberString(servo_id, display_base, 8, id_str, sizeof(id_str));

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
		if ((pregister_name = GetServoRegisterName(servo_id, reg_start)))
		{
			ss <<" - ";
			ss << pregister_name;
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
				if ((index_data_byte < (count_data_bytes - 1)) && IsServoRegisterStartOfPair(servo_id, reg_start + index_data_byte))
				{
					// need to special case index 4 as it splits the two mdata members
					U16 wval;
					if (index_data_byte == 4) {
						wval = ((frame.mData2 & 0xff) << 8) | (shift_data & 0xff);
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

			if ((pregister_name = GetServoRegisterName(servo_id, reg_start)))
			{
				ss << " - " << pregister_name;
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
		if ((pregister_name = GetServoRegisterName(servo_id, reg_start)))
		{
			ss << " - " << pregister_name;
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

			if ((index_data_byte < (count_data_bytes - 1)) && IsServoRegisterStartOfPair(servo_id, reg_start + index_data_byte))
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
			if (count_data_bytes > 12)
				count_data_bytes = 12;	// Max bytes we can store
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

const char * DynamixelAnalyzerResults::GetServoRegisterName(U8 servo_id, U16 register_number)
{
	// See if this is our controller servo ID
	if (servo_id == mSettings->mServoControllerID)
	{
		// Now only processing CM730...  special...
		if (servo_id == CONTROLLER_CM730ISH)
		{
			if (register_number < (sizeof(s_cm730_register_names) / sizeof(s_cm730_register_names[0])))
				return s_cm730_register_names[register_number];
		}
		return NULL;
	}

	// Else process like normal servos
	switch (mSettings->mServoType)
	{
	case SERVO_TYPE_AX:
		if (register_number < (sizeof(s_ax_register_names) / sizeof(s_ax_register_names[0])))
			return s_ax_register_names[register_number];
		break;

	case SERVO_TYPE_MX:
		if (register_number < (sizeof(s_mx_register_names) / sizeof(s_mx_register_names[0])))
			return s_mx_register_names[register_number];
		break;

	case SERVO_TYPE_XL:
		if (register_number < (sizeof(s_xl_register_names) / sizeof(s_xl_register_names[0])))
			return s_xl_register_names[register_number];
		break;
	}
	return NULL;
}


bool DynamixelAnalyzerResults::IsServoRegisterStartOfPair(U8 servo_id, U16 register_number)
{
	// See if this is our controller servo ID
	if (servo_id == mSettings->mServoControllerID)
	{
		// Now only processing CM730...  special...
		if (servo_id == CONTROLLER_CM730ISH)
		{
			return (mSettings->mShowWords && (register_number < sizeof(s_is_cm730_register_pair_start)) && s_is_cm730_register_pair_start[register_number]);
		}
		return NULL;
	}

	// Else process like normal servos
	switch (mSettings->mServoType)
	{
	case SERVO_TYPE_AX:
		return (mSettings->mShowWords && (register_number < sizeof(s_is_ax_register_pair_start)) && s_is_ax_register_pair_start[register_number]);

	case SERVO_TYPE_MX:
		return (mSettings->mShowWords && (register_number < sizeof(s_is_mx_register_pair_start)) && s_is_mx_register_pair_start[register_number]);

	case SERVO_TYPE_XL:
		return (mSettings->mShowWords && (register_number < sizeof(s_is_xl_register_pair_start)) && s_is_xl_register_pair_start[register_number]);
		break;
	}
	return false;
}


