#include<MIPS_Processor.hpp>
#include<map>
#include<string>
#include<set>
#include<vector>
#include<iostream>

using namespace std;
#define pint pair<int,int>
map<string,pair<int,int>> DataHazards;
bool jumpStall = false;
int branchStall = 0;
int stallNumber = 0;
set<int> pcs;
struct IFID //basically the L2 latch, used to transfer values between IF and ID stage
{
	vector<string> currentCommand = {};
	vector<string> nextCommand = {};
	int curPc, nextPc;
	IFID()
	{
		currentCommand = {};
		nextCommand = currentCommand;
	}
	void Update()
	{
		currentCommand = nextCommand; 
		curPc = nextPc;
		nextPc = 0;
		nextCommand = {};
	}
};

struct IF0
{
	MIPS_Architecture *arch;
	IFID *LIF;
	IF0(MIPS_Architecture *mips, IFID *lif)
	{
		arch = mips;
		LIF = lif;
	}

	void run()
	{
		//checks if we are out of instructions
		if(arch->outputFormat == 0)
			cout << "|IF0|=>";

		//checks if we are supposed to stall
		if(stallNumber > 0)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			
			return;
		}
		if(branchStall > 0)
		{
			if(!arch->outputFormat)
				cout << "**";
			return;
		}
		if(arch->PCnext >= arch->commands.size())
		{
			if(!arch->outputFormat)
				cout << "done";
			return;
		}
		arch->PCcurr = arch->PCnext; arch->PCnext++;
		if(arch->outputFormat==0)
			cout << "fetched: " << arch->PCcurr;
		pcs.insert(arch->PCcurr); //inserted the pc into the set
		//else we will work
		//then we check if the current instruction is a branch
		LIF->nextPc = arch->PCcurr;
		LIF->nextCommand = arch->commands[arch->PCcurr];
		if(arch->commands[arch->PCcurr][0] == "beq" || arch->commands[arch->PCcurr][0] == "bne" || arch->commands[arch->PCcurr][0] == "j")
		{
			//then we need to stall the pipeline
			branchStall = 1; //so the next IF instruction gets stalled
			//and pass the commands forward as well
		}
	}
};

struct IF1
{
	MIPS_Architecture *arch;
	IFID *LIF, *L2;
	IF1(MIPS_Architecture *mips, IFID *lif, IFID *l2)
	{
		arch = mips;
		LIF = lif;
		L2 = l2;
	}

	void run()
	{
		if(arch->outputFormat==0)
			cout << "|IF1|=>";

		if(stallNumber > 1)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			LIF->nextPc = LIF->curPc;
			LIF->nextCommand = LIF->currentCommand;			
			return;
		}
		if(branchStall > 1)
		{
			if(!arch->outputFormat)
				cout << "**";
			LIF->nextPc = LIF->curPc;
			LIF->nextCommand = LIF->currentCommand;		
			return;
		}
		//else we will work
		
		L2->nextCommand = LIF->currentCommand;
		if(LIF->currentCommand.size() == 0)
		{
			//then actually it hasnt been passed a command yet, so we just return
			return;
		}
		L2->nextPc = LIF->curPc;
		if(arch->outputFormat==0)
			cout << "fetched1 " << LIF->curPc;
		if(LIF->currentCommand[0] == "beq" || LIF->currentCommand[0] == "bne" || LIF->currentCommand[0] == "j")
		{
			branchStall = 2; //so the next IF1 instruction gets stalled as well.
		}
	} 
};

struct IDID //the  latch lying between the latch lying between ID0 and ID1
{
	// vector<int> curData, nextData;
	vector<string> curCommand = {}, nextCommand = {};
	int curPc, nextPc;
	void Update()
	{
		//on getting the updated values, we can run the code
		// curData = nextData;
		curCommand = nextCommand;
		curPc = nextPc;
		nextCommand = {};
		//nextInstructionType = ""; //this would ensure that if instruction
		// type does not get updated, then we won't run any new commands
	}
};

struct ID0
{
	MIPS_Architecture *arch;
	IFID *L2; IDID *L3;
	//ID0 will be responsible for decoding the instruction
	ID0(MIPS_Architecture *mips, IFID *l2, IDID *l3)
	{
		arch = mips;
		L2 = l2;
		L3 = l3;
	}
	void run()
	{
		if(arch->outputFormat==0)
			cout << "|ID0|=>";
		if(stallNumber > 2)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			L2->nextPc = L2->curPc;
			L2->nextCommand = L2->currentCommand;
			return;
		}
		if(branchStall > 2)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			L2->nextPc = L2->curPc;
			L2->nextCommand = L2->currentCommand;
			return;
		}
		//else we will work
		L3->nextCommand = L2->currentCommand;
		if(L2->currentCommand.size() == 0)
		{ 	//we haven't been passed a command yet, so we just return. This is basically a no-op
			return;
		}
		L3->nextPc = L2->curPc;
		if(L2->currentCommand[0] == "beq" || L2->currentCommand[0] == "bne" || L2->currentCommand[0] == "j")
		{
			//then we need to stall the pipeline
			branchStall = 3; //so the next ID0 instruction gets stalled as well.
			L2->currentCommand = {};
			//and pass the commands forward as well
		}
	}
};

struct IDRR
{	public:
	vector<string> curCommand = {}, nextCommand = {};
	int curOffset = 0, nextOffset = 0;
	int nextPc= -1, curPc = -1;
	map<string,pair<int,int>> nextDataHazards, curDataHazards; 

	void Update()
	{
		curCommand = nextCommand;
		curPc = nextPc;
		nextCommand = {};
		curOffset = nextOffset;
		curDataHazards = nextDataHazards;
	}
};

struct ID1
{
	MIPS_Architecture *arch;
	IDID *LID; 	IDRR *L4; 
	vector<int> whichLatch = vector<int>(3,0); //used for forwarding.
	vector<string> InstructionsLeft = vector<string>(4,""); //stores what type of instructions have previously left the ID1 stage
	//useful for determining if a 9 stage instruction that left before will clash with the current instruction  at the writeback s
	//stage if they both use the writeback port
	vector<string> curCommand = {}; string instructionType;
	//ID0 will be responsible for decoding the instruction
	ID1(MIPS_Architecture *mips, IDID *lid, IDRR *l4)
	{
		arch = mips;
		LID = lid;
		L4 = l4;
	}
	bool checkForFIFOstall(bool willWrite)
	{
		if(InstructionsLeft[1] == "lw" || InstructionsLeft[1] == "sw")
		{
			//then we need to stall regardless of whether the current instruction will write or not
			return true;
		}
		if(willWrite)
		{
			if(InstructionsLeft[2] == "lw")
			{
				//then we need to stall
				return true;
			}
			else
			{
				return false; //we don't need to stall
			}
		}
		else
		{
			return false; //we don't need to stall since we will not write and an instruction 2 stage before can use writeback stage
		}
	}
	void UpdateInstructionsLeft()
	{
		for (int i = InstructionsLeft.size() - 2; i >= 0; i--)
		{
			InstructionsLeft[i+1] = InstructionsLeft[i]; //shift everything to the right
		}
		InstructionsLeft[0] = "";
	}
	void run()
	{
		L4->nextDataHazards = DataHazards; //passing the entire datahazards ahead.
		whichLatch = vector<int>(3,0);
		if(arch->outputFormat==0)
			cout << "|ID1|=>";
		//first we check the stall condition
		UpdateInstructionsLeft(); //moving all the previous instructions to the right
		if(stallNumber > 3)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			LID->nextPc = LID->curPc;
			LID->nextCommand = LID->curCommand;
			return;
		}
		
		else if(branchStall > 3)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			LID->nextPc = LID->curPc;
			LID->nextCommand = LID->curCommand;
			return;
		}
		else
		{
			curCommand = LID->curCommand;
		}
		//else we will work
		//we will first check if the instruction is a branch, sent from IF1
		if(curCommand.size() == 0)
		{
			L4->nextCommand = curCommand; //passing a no-op
			return;
		}
		instructionType = curCommand[0];
		if(instructionType == "j")
		{
			//then we needa jump to
			L4->nextCommand = {}; //passing a no-op;
			arch->j(curCommand[1],"",""); //this moves the pc
			LID->curCommand = {};
			L4->nextPc = LID->curPc;
			//also we need to set the new PC now, and also change branchstall.
			jumpStall = true;
		}

		if (LID->curCommand[0] == "lw" || LID->curCommand[0] == "sw")
		{
			//then we need to do address calculation as well, so we parse the address first
			pair<int,string> val = arch->decodeAddress(curCommand[2]);	
			L4->nextOffset = val.first;
			curCommand[2] = val.second; //we replace the address with the register name
			//now we check for data hazards
			bool shouldStall = (LID->curCommand[0] == "sw" && (DataHazards.count(curCommand[1]) && DataHazards[curCommand[1]].first - DataHazards[curCommand[1]].second <= 3));
			shouldStall = shouldStall || (DataHazards.count(curCommand[2]) && DataHazards[curCommand[2]].first - DataHazards[curCommand[2]].second <= 3);
			
			
			if(shouldStall)
			{
				//then we need to stall the pipeline
				stallNumber = 3; //so the next ID1 instruction gets stalled as well. //then we stall.
				LID->nextCommand = LID->curCommand;
				LID->nextPc = LID->curPc;
				L4->nextPc = LID->curPc;
				//and do nothing else
				//and pass the commands forward as well
				return; //we return as there is nothing to do. the next stages automatically recieve a no-op
			}
			else stallNumber = 0; //if we're not stalling
			InstructionsLeft[0] = instructionType; //updated with the current instruction.
		}
		else if(LID->curCommand[0] == "beq" || LID->curCommand[0] == "bne")
		{
			
			bool shouldStall = (DataHazards.count(curCommand[3]) && DataHazards[curCommand[3]].first - DataHazards[curCommand[3]].second <= 3);
			shouldStall = shouldStall || (DataHazards.count(curCommand[2]) && DataHazards[curCommand[2]].first - DataHazards[curCommand[2]].second <= 3);
			shouldStall = (shouldStall || checkForFIFOstall(false));
			if(shouldStall)
			{
				//then we need to stall the pipeline
				stallNumber = 3; //so the next ID1 instruction gets stalled as well. //then we stall.
				LID->nextCommand = LID->curCommand;
				LID->nextPc = LID->curPc;
				//and do nothing else //and pass the commands forward as well
				return; //we return as there is nothing to do. the next stages automatically recieve a no-op
			}
			//then we need to stall the pipeline
			branchStall = 4; //so the next ID1 instruction gets stalled as well.
			stallNumber = 0;
			LID->curCommand = {};
			InstructionsLeft[0] = instructionType; //updated with the current instruction.
			//and do nothing else
			//and pass the commands forward as well	
		}
		else
		{
			bool shouldStall = false;
			//either it is num 0 or num 1 type instruction, both of which have the first register as a dataHazard.
			if(arch->instructionNumber(instructionType) == 0) 	//check dependency for the second register
				if(DataHazards.count(curCommand[3]) && DataHazards[curCommand[3]].first - DataHazards[curCommand[3]].second <= 3)
				{
					//first we check if the value can be forwarded ahead, 
					whichLatch[1] = DataHazards[curCommand[3]].first; //cout << "this latch" << DataHazards[curCommand[3]].first << endl;
					shouldStall = true;
				}
					
			
			if(DataHazards.count(curCommand[2]) && DataHazards[curCommand[2]].first - DataHazards[curCommand[2]].second <= 3)
				shouldStall = true;
			
			if(checkForFIFOstall(true))
				shouldStall = true;
			
			if(shouldStall)
			{
				//then we need to stall the pipeline
				stallNumber = 3; //so the next ID1 instruction gets stalled as well. //then we stall.
				LID->nextCommand = LID->curCommand;
				LID->nextPc = LID->curPc;
				//and do nothing else
				//and pass the commands forward as well
				return; //we return as there is nothing to do. the next stages automatically recieve a no-op
			}
			//otherwise we anyway have to check the dependency for the first register
			stallNumber = 0; //reset the stall number otherwise
			
		}
		if(instructionType != "sw" && instructionType != "beq" && instructionType != "bne" && instructionType != "j")
		{
			DataHazards[curCommand[1]].first = 3;
			DataHazards[curCommand[1]].second = (instructionType == "lw" ? 2 : 0); //the datahazard is inserted here
		}
		L4->nextPc = LID->curPc; L4->nextCommand = curCommand; InstructionsLeft[0] = instructionType; //updated with the current instruction.
	}
};
struct RREX //the latch lying between RR and EX
{
	vector<int> curData = vector<int>(3,0), nextData = vector<int>(3,0);
	vector<string> curCommand,nextCommand;
	string curWriteReg, nextWriteReg;
	map<string, pair<int,int>> curDataHazards, nextDataHazards;
	int curPC = -1, nextPC = 0;
	void Update()
	{
		//on getting the updated values, we can run the code
		curData = nextData;
		curCommand = nextCommand;
		curWriteReg = nextWriteReg;
		nextCommand = {};
		curPC = nextPC;
		curDataHazards = nextDataHazards;
		//nextInstructionType = ""; //this would ensure that if instruction
		// type does not get updated, then we won't run any new commands
	}
};


struct RR
{
	MIPS_Architecture *arch;
	IDRR *L4; RREX *L5r, *L5i;
	vector<int> regVal; int nextOffset = 0;
	string writeReg = "";
	vector<string> curCommand;
	//RR is responsible for reading the register values and passing them to EX for working
	RR(MIPS_Architecture *mips, IDRR *l4, RREX *l5a, RREX *l5b)
	{
		arch = mips;
		regVal = vector<int>(3,0);
		L4 = l4;
		L5r = l5a;
		L5i = l5b;
	}
	void run()
	{
		L5i->nextDataHazards = L4->curDataHazards;
		L5r->nextDataHazards = L4->curDataHazards;
		if(arch->outputFormat==0)
			cout << "|RR|=>";
		if(stallNumber > 4)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			return;
		}
		else if(branchStall > 4)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			return;
		}
		else
		{
			curCommand = L4->curCommand;
		}
		//else we will work
		if(curCommand.size() == 0)
		{

			// L4->nextCommand = curCommand; //passing a no-op
			return;
		}
		
		regVal[0] = arch->registers[arch->registerMap[curCommand[2]]];
		if(curCommand[3] == "")
			regVal[1] = 0;
		else if(arch->instructionNumber(curCommand[0]) == 0)
			regVal[1] = arch->registers[arch->registerMap[curCommand[3]]];
		else if(arch->instructionNumber(curCommand[0]) == 1)
			regVal[1] = stoi(curCommand[3]);
		// else
		// 	regVal[1] = arch->registers[arch->registerMap[curCommand[3]]]; //getting its value normally

		writeReg = curCommand[1]; 		
		if(arch->instructionNumber(curCommand[0]) == 2)
		{
			//then we need to take the 9 stage pipeline path
			if(!arch->outputFormat)
				cout << "Itype ";
			nextOffset = L4->curOffset;
			regVal[1] = nextOffset; 
			regVal[2] = arch->registers[arch->registerMap[curCommand[1]]]; //getting the value of the register
			// L5r->nextCommand = {}; //passing a no-op
			L5i->nextPC = L4->curPc;
			L5i->nextWriteReg = writeReg;
			L5i->nextData = regVal; //passing the data to ALU of the i type (9 stage) instruction
			L5i->nextCommand = curCommand; 
			if(arch->outputFormat == 0)
			cout << curCommand[0] << " " << nextOffset << "+" << regVal[0] << "for " << curCommand[1] <<":" << regVal[2] ; // << "data-" <<  << " ";
		}
		else
		{
			//otherwise we need to take the the 7 stage pipeline path
			// L5i->nextCommand = {};
			L5r->nextPC = L4->curPc;
			L5r->nextCommand = curCommand;
			L5r->nextWriteReg = writeReg;
			L5r->nextData = regVal; //passing the data to ALU of the r type (7 stage) instruction
			if(curCommand[0] == "beq" || curCommand[0] == "bne")
			{
				//then we need to stall the pipeline
				curCommand = {};
				L5r->nextData[0] = arch->registers[arch->registerMap[curCommand[1]]];
				L5r->nextData[1] = arch->registers[arch->registerMap[curCommand[2]]];
				branchStall = 5; //so the next RR instruction gets stalled as well.
				//and pass the commands forward as well	
				if(!arch->outputFormat)
					cout << "sent branch values ";
				
			}
			else if(!arch->outputFormat) {
				cout << "Rtype ";
				cout << "passed " << regVal[0] << " " << regVal[1] << " "; 
			}
		}
	}
};

struct EXDM
{
	vector<string> curCommand, nextCommand;
	string curReg, nextReg;
	int curSWdata, nextSWdata;
	int curAddr, nextAddr;
	int curPC = -1, nextPC = -1;
	map<string, pint> curDataHazards, nextDataHazards;
	void Update()
	{
		// curAddr = nextAddr; curReg = nextReg;
		curCommand = nextCommand; nextCommand = {};
		curAddr = nextAddr;
		curReg = nextReg; 
		curSWdata = nextSWdata;
		curPC = nextPC; nextPC = -1;
		curDataHazards = nextDataHazards;
	}
};
struct LWB
{
	string curReg, nextReg;
	int curDataOut, nextDataOut;
	bool curIsUsingWriteBack = false, nextIsUsingWriteBack = false;
	int curPC = -1, nextPC = -1;

	void Update()
	{
		curReg = nextReg; curDataOut = nextDataOut;
		nextReg = "";
		curIsUsingWriteBack = nextIsUsingWriteBack;
		nextIsUsingWriteBack = false;
		curPC = nextPC; nextPC = -1;
	}
};
struct DM0
{
	MIPS_Architecture *arch;
	EXDM *L7, *L8; //L8 here will be used for to transferring data from this stage to the DM1 stage
	DM0(MIPS_Architecture *architecture, EXDM *l7, EXDM *l8)
	{
		arch = architecture; L7 = l7; L8 = l8;
	}
	void run()
	{
		 //transporting the value from the DM0 stage to the DM1 stage, where all the computation will happen
		if(arch->outputFormat==0)
			cout << "|DM0|=>";	
		L8->nextPC = L7->curPC; //PC update
		L8->nextAddr = L7->curAddr;
		L8->nextCommand = L7->curCommand;
		L8->nextReg = L7->curReg;
		L8->nextSWdata = L7->curSWdata; 							
	}
};
struct DM1
{
	MIPS_Architecture *arch;
	EXDM *L8; LWB *L6; int Addr = 0;
	string writeReg = ""; bool memWrite = false;
	DM1(MIPS_Architecture *architecture, EXDM *l8, LWB *l6)
	{
		arch = architecture; L8 = l8; L6 = l6;
	}
	void run()
	{
		memWrite = false;
		if(arch->outputFormat==0)
			cout << "|DM1|=>";

		if(L8->curCommand.size() == 0)
		{
			// L6->nextIsWorking = false;
			return;
		}
		memWrite = (L8->curCommand[0] == "sw");
		Addr = L8->curAddr;
		L6->nextPC = L8->curPC;
		if(L8->curCommand[0] == "lw")
		{
			L6->nextReg = L8->curReg;
			L6->nextIsUsingWriteBack = true;
			L6->nextDataOut = arch->data[Addr];
			if(!arch->outputFormat)
				cout << "lw " << L8->curReg << " " << L6->nextDataOut << " ";
		}
		else if(L8->curCommand[0] == "sw")
		{
			arch->data[Addr] = L8->curSWdata;
			L6->nextIsUsingWriteBack = false;
			if(!arch->outputFormat)
				cout << "sw " << L8->curSWdata << " " << Addr << " ";
		}
	}
};

struct afterWB //afterWB is used for forwarding values behind, so we do not lose those values. assuming that reading registers is much more costly than reading from latches
{
	int address = 0;
	int dataOut = 0;
};

struct EX
{	
	public:
	int swData;
	MIPS_Architecture *arch; RREX *L5; EXDM *L7; LWB *L6; afterWB *awb; LWB *aDM;
	string iType = "";
	vector<int> dataValues; 
	map<string,pint> DH;
	int result = 0;
	string r0; //register to be written into, this will not be used in this step but passed forward till the WriteBack stage where it will be written into
	//now we decode the instruction from the instructions map
	int checkforPC;
	EX(MIPS_Architecture *architecture, RREX *l5, EXDM *l7, LWB *l6, afterWB *l9, LWB *adm)
	{
		arch = architecture; L5 = l5; L7 = l7; L6 = l6; awb = l9; aDM = adm;//the latch reference and architecture reference is stored at initialization
		dataValues = vector<int>(3,0);
	}

	void run()
	{	
		DH = L5->curDataHazards;
		if(arch->outputFormat == 0)
			cout << "|EX|=>";
		if(stallNumber > 5)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			return;
		}
		if(branchStall > 5)
		{
			//then we are supposed to stall and effectively do nothing
			if(!arch->outputFormat)
				cout << "**";
			return;
		}
		else
		{
			if(L5->curCommand.size() == 0)
			{
				return; //a no-op
			}
			iType = L5->curCommand[0];
			dataValues = L5->curData; //getting the data from L3 in the nonforwarding case
			r0 = L5->curCommand[1];   //the register to be written into
		}
		
		//else we will work
		if(arch->instructionNumber(L5->curCommand[0]) == 2)
		{
			//then this EX is of the 9 stage pipeline path
			if(DH.count(L5->curCommand[2]))
			{
				//then the value of dataValues[0];
				if(DH[L5->curCommand[2]].second == 0)
				{
					switch (DH[L5->curCommand[2]].first)
					{
					case 4:
						dataValues[0] = L6->curDataOut; //this is the forwarded value from the register between EX and WB
						break;
					case 5:
						dataValues[0] = awb->dataOut;
						break;
					default:
						break;
					}
				}
				else
				{
					switch(DH[L5->curCommand[2]].first)
					{
						case 6:
							dataValues[0] = aDM->curDataOut;

						default: 
						break;
					}
				}
			}
			
			if(L5->curCommand[0] == "sw")
			{
				if(DH.count(L5->curCommand[1]))
				{
					//the register from which the value is taken is also a data hazard, hence we must forward its value
					if(DH[L5->curCommand[1]].second == 0)
					{
						switch (DH[L5->curCommand[1]].first)
						{
						case 4:
							dataValues[2] = L6->curDataOut; //this is the forwarded value from the register between EX and WB
							break;
						case 5:
							dataValues[2] = awb->dataOut;
							break;
						default:
							break;
						}
					}
					else
					{
						switch(DH[L5->curCommand[1]].first)
						{
							case 6:
								dataValues[2] = aDM->curDataOut;
							
							default: 
							break;
						}
					}
				}
			}
			//another case for wrong output is when we have sw and want to store the word, in that case the current needed word can be found

			L7->nextDataHazards = L5->curDataHazards;
			L7->nextPC = L5->curPC; //PC update
			//in this case the address can be calculated by adding dataValues[0] and dataValues[1] 
			//and dataValues[2] will be the data to be written into the memory incase of sw
			int address = dataValues[0] + dataValues[1]; //this is indeed the address
			if(address%4 != 0) cerr << "Error: Address not word aligned" << endl;
				address = address/4;
			if(arch->outputFormat == 0) cout << "address: " << address << " " << "<-" << dataValues[2];
			L7->nextCommand = L5->curCommand;
			L7->nextAddr = address;
			L7->nextReg = r0;
			L7->nextSWdata = dataValues[2]; //this is the data to be written into the memory incase of sw       
		}
		else
		{
			//then this EX is of the 7stage pipeline path
			
			if(L5->curCommand[0] == "beq" || L5->curCommand[0] == "bne")
			{

				if(DH.count(L5->curCommand[2]))
				{
					//then the value of dataValues[0];
					if(DH[L5->curCommand[2]].second == 0)
					{
						switch (DH[L5->curCommand[2]].first)
						{
						case 4:
							dataValues[1] = L6->curDataOut; //this is the forwarded value from the register between EX and WB
							break;
						case 5:
							dataValues[1] = awb->dataOut;
						default:
							break;
						}
					}
					else
					{
						switch(DH[L5->curCommand[2]].first)
						{
							case 6:
								dataValues[1] = aDM->curDataOut;

							default: 
							break;
						}
					}
				}	
				if(DH.count(L5->curCommand[1]))
				{
					//then the value of dataValues[0];
					if(DH[L5->curCommand[1]].second == 0)
					{
						switch (DH[L5->curCommand[1]].first)
						{
						case 4:
							dataValues[0] = L6->curDataOut; //this is the forwarded value from the register between EX and WB
							break;
						case 5:
							dataValues[0] = awb->dataOut;
						default:
							break;
						}
					}
					else
					{
						switch(DH[L5->curCommand[1]].first)
						{
							case 6:
								dataValues[0] = aDM->curDataOut;

							default: 
							break;
						}
					}
				}


				branchStall = 0; stallNumber = 0;
				if(arch->outputFormat==0) cout << dataValues[0] << "=?" << dataValues[1] << " ";
				if((L5->curCommand[0] == "bne")^(dataValues[0] == dataValues[1]))
				{
					//then we branch
					L6->nextPC = L5->curPC; //PC update
					arch->j(L5->curCommand[3],"",""); //this moves the pc
					//cout << curCOmm
					L6->nextIsUsingWriteBack = false;
					if(!arch->outputFormat)
						cout << "branched " << L6->nextPC << " ";
					return;
				}
				else
				{
					//then we do not branch
					L6->nextPC = L5->curPC; //PC update
					L6->nextIsUsingWriteBack = false;
					if(!arch->outputFormat)
						cout << "not branch " << L6->nextPC << " ";
					return;
				}
			}
			if(DH.count(L5->curCommand[2]))
			{
				//then the value of dataValues[0];
				if(DH[L5->curCommand[2]].second == 0)
				{
					switch (DH[L5->curCommand[2]].first)
					{
					case 4:
						dataValues[0] = L6->curDataOut; //this is the forwarded value from the register between EX and WB
						break;
					case 5:
						dataValues[0] = awb->dataOut;
					default:
						break;
					}
				}
				else
				{
					switch(DH[L5->curCommand[2]].first)
					{
						case 6:
							dataValues[0] = aDM->curDataOut;

						default: 
						break;
					}
				}
			}			
			if(arch->instructionNumber(L5->curCommand[0]) == 0)
			{
				if(DH[L5->curCommand[3]].second == 0)
				{
					switch (DH[L5->curCommand[3]].first)
					{
					case 4:
						dataValues[1] = L6->curDataOut; //this is the forwarded value from the register between EX and WB
						break;
					case 5:
						dataValues[1] = awb->dataOut;
					default:
						break;
					}
				}
				else
				{
					switch(DH[L5->curCommand[3]].first)
					{
						case 6:
							dataValues[1] = aDM->curDataOut;

						default: 
						break;
					}
				}
			}
			L6->nextPC = L5->curPC; //PC update
			int result = calc();
			if(arch->outputFormat == 0) cout << "Result: " << result << " ";
			L6->nextIsUsingWriteBack = true;
			L6->nextReg = r0;
			L6->nextDataOut = result;
		}
	}
	int calc()
	{
		if(iType == "") return -1;
		if(iType == "add" || iType == "addi" || iType == "lw" || iType == "sw")
			return dataValues[0] + dataValues[1];
		else if(iType == "sub")
			return dataValues[0] - dataValues[1];
		else if(iType == "mul")
			return dataValues[0] * dataValues[1];
		else if(iType == "and" || iType == "andi")
			return (dataValues[0] & dataValues[1]);
		else if(iType == "or" || iType == "ori")
			return (dataValues[0] | dataValues[1]);
		else if(iType == "srl")
			return (dataValues[0] >> dataValues[1]);
		else if(iType == "sll")
			return (dataValues[0] << dataValues[1]);
		else  //if slt
			return (dataValues[0] < dataValues[1]); 
	}

};

struct WB
{	public:
	MIPS_Architecture *arch; LWB *dmwb, *exwb, *usingLatch; afterWB *awb;
	int dataOut = 0; string reg = ""; int curPc = -1;
	WB(MIPS_Architecture *architecture, LWB *lwb1, LWB *lwb2, afterWB *awb1)
	{
		arch = architecture; dmwb = lwb1; exwb = lwb2; awb = awb1;
	}
	void run()
	{
		if(arch->outputFormat == 0)
			cout << "|WB|=> ";
		//check which one of these requires the writeback port, or if none require it.
		pcs.erase(dmwb->curPC); pcs.erase(exwb->curPC); 
		if(dmwb->curIsUsingWriteBack && !(exwb->curIsUsingWriteBack))
		{
			usingLatch = dmwb;
		}
		else if(exwb->curIsUsingWriteBack && !(dmwb->curIsUsingWriteBack))
		{
			usingLatch = exwb;
		}
		else if(!(dmwb->curIsUsingWriteBack) && !(exwb->curIsUsingWriteBack))
		{
			awb->address = -1; awb->dataOut = -1; //if both are false, then there is actually no instruction here at all
			return; //do nothing this cycle
		}
		else
		{
			//then both are using the writeback port, so we would have stalled before in ID stage
			//so we do nothing
			// cerr << "both writing??";
		}
		//erasing both of those pcs
		reg = usingLatch->curReg; dataOut = usingLatch->curDataOut;
		curPc = usingLatch->curPC; //with this we get the pc 
		awb->dataOut = dataOut;
		if(arch->outputFormat == 0)
			cout << "pcI:" << dmwb->curPC << "pcR:" << exwb->curPC  << " ";
		if(reg != "")
		{
			arch->registers[arch->registerMap[reg]] = dataOut;
			if(arch->outputFormat == 0)
				cout << reg << ":" << dataOut << " ";
		}
	}
};

void HazardUpdate(int maxVal)
	{
		auto i = DataHazards.begin();
		while(i != DataHazards.end())
		{
			if(++(i->second.first) >= maxVal)
			{
				auto t = i;
				i++; DataHazards.erase(t);
			}
			else{
				i++;
			}
		}
	}
void setJumpStall()
{
	if(jumpStall)
	{
		branchStall = 0;
		jumpStall = false;
	}
	
}

void ExecutePipelined(MIPS_Architecture *arch)
	{
		if (arch->commands.size() >= arch->MAX / 4)
		{
			arch->handleExit(arch->MEMORY_ERROR, 0);
			return;
		} //memory error

		//registers[registerMap["$sp"]] = (4 * commands.size()); //initializes position of sp. assumes that all the commands are also stored in data and so sp needs to be here
		//the above is optional, but since none of the testcases utilize it, it has been commented out
		int clockCycles = 0;
		//first we instantiate the stages
		IFID L2, LIF; //The Latches
		IDID L3; IDRR L4; RREX L5i, L5r; //The Latches
		EXDM L7, L9; LWB L6r, L8i; //The Latches 
		IF0 fetch0(arch,&LIF); IF1 fetch1(arch,&LIF,&L2);
		ID0 decode0(arch,&L2,&L3);
		ID1 decode1(arch,&L3,&L4); afterWB awb;
		RR readReg(arch, &L4, &L5i, &L5r); //IDRR (arch,&L4);
		EX ALUi(arch,&L5i,&L7, &L6r, &awb, &L8i); //RREX (arch,&L5);
		EX ALUr(arch,&L5r,&L7, &L6r, &awb, &L8i); //RREX (arch,&L5);
		WB writeBack(arch,&L8i,&L6r, &awb); //LWB (arch,&L6);
		DM0 dataMem0(arch,&L7,&L9); //EXDM (arch,&L7);
		DM1 dataMem1(arch,&L9,&L8i); //LWB (arch,&L8);
		arch->printRegisters(clockCycles); //initial state
		cout << 0 << endl;
		int i = 12;
		do
		{
			setJumpStall();
			decode1.run();
			decode0.run(); 
			fetch0.run(); 
			fetch1.run();  
			readReg.run();
			ALUi.run(); ALUr.run();
			dataMem0.run(); dataMem1.run();
			writeBack.run();

			L2.Update(); 
			LIF.Update(); 
			L3.Update(); 
			L4.Update();
			L5i.Update();
			L5r.Update();
			L6r.Update();
			L7.Update();
			L8i.Update();
			L9.Update();
			if(arch->outputFormat == 0) 
			{	
				std::cout << " dataHazards are : ";
				for(auto i: DataHazards)
				{	
					std::cout << i.first << " " << i.second.first <<   ", ";
				}
			}	
			clockCycles++;
			arch->printRegisters(clockCycles);

			if(dataMem1.memWrite)
			{
				cout << 1 << " "<< dataMem1.Addr << " " << dataMem1.L8->curSWdata;
			}
			else
			{
				cout << 0;
			}
			
			HazardUpdate(8); //updating the hazards
		
			if(arch->outputFormat == 0)
			{
				cout << "^";
				for (auto i: pcs)
				{
					cout << i << ".";
				}
			}
			
			//cout << endl << " at clockCycles " << clockCycles << endl;
			std::cout << endl;
			
			
		} while((pcs.size() > 0));
		arch->handleExit(arch->SUCCESS, clockCycles);

	}

//here the commands are being actually executed.
int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		std::cerr << "Required argument: file_name\n./MIPS_interpreter <file name>\n";
		return 0;
	}
	std::ifstream file(argv[1]);
	MIPS_Architecture *mips;
	if (file.is_open())
		mips = new MIPS_Architecture(file);
	else
	{
		std::cerr << "File could not be opened. Terminating...\n";
		return 0;
	}

	ExecutePipelined(mips);
	return 0;
}

