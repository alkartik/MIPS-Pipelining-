#include<MIPS_Processor.hpp>
#include<map>
#include<string>
using namespace std;
map<string,pair<int,int>> DataHazards; //the first of the pair is the latch number and the second one is the type of instruction




struct IFID //basically the L2 latch, used to transfer values between IF and ID stage
{
	vector<string> currentCommand = {};
	vector<string> nextCommand = {};
	bool IDisStalling = false;
	bool curIsWorking = true, nextIsWorking = true; 
	int PC;
	int PCrun;
	IFID()
	{
		currentCommand = {};
		nextCommand = currentCommand;
		PCrun = PC;
	}
	void Update()
	{
		currentCommand = nextCommand; 
		curIsWorking = nextIsWorking;
		PC = PCrun;
	}
};
struct IF
{
	public:
	MIPS_Architecture *arch;
	bool isWorking = true;
	IFID *L2; //L2 latch
	int address;
	vector<string> CurCommand; //the current command after reading the address
	
	IF(MIPS_Architecture *architecture, IFID *l2)
	{
		arch = architecture; L2 = l2; isWorking = true;
	}
	void run()
	{
		if(arch->outputFormat == 0)
		cout << " |IF|=> ";
		if(arch->PCnext >= arch->commands.size())
		{
			L2->nextCommand = {};
			isWorking = false;
			L2->nextIsWorking = false;
			return; //since we must be done with all the commands at this point
		} 
		if(L2->IDisStalling == false)
		{
			arch->PCcurr = arch->PCnext;
			arch->PCnext++;
			++arch->commandCount[arch->PCcurr];
			address = arch->PCcurr;
			if(arch->outputFormat == 0)
				cout << "Fetched Command No. " << arch->PCcurr;
			L2->PCrun = arch->PCcurr;
			CurCommand = arch->commands[address]; //updates to this address
			L2->nextCommand = CurCommand; //updates the value in the L2 at the same time, but for the next time
		}
		else
		{
			if(arch->outputFormat == 0)
				cout << "** ";
		}
	}
};
int instructionNumber(string s)
{
	if(s == "add" || s == "and" || s == "sub" || s == "mul" || s == "or" || s == "slt")
		return 0;
	else if(s == "addi" || s == "andi" || s == "ori" || s == "srl" || s == "sll")
		return 1;
	else if(s == "lw" || s == "sw")
		return 2;
	return 3;
}
struct IDEX //the L3 register lying between ID and EX
{
	vector<int> curData, nextData;
	string curWriteReg = "", nextWriteReg = "";
	string curInstructionType = "", nextInstructionType = "";
	bool IDisStalling = false;
	bool curIsWorking = true, nextIsWorking = true;
	int curIsBranch = 0, nextIsBranch = 0; //0 means not a branch value, 1 means beq and 2 means bne
	vector<int> curWhichLatch = vector<int>(3,0), nextWhichLatch = vector<int>(3,0);
	int PC;
	int PCrun;
	string secondregister="";
	void Update()
	{
		//on getting the updated values, we can run the code
		curData = nextData;
		curInstructionType = nextInstructionType; curWriteReg = nextWriteReg;
		curIsWorking = nextIsWorking;
		PC = PCrun;
		nextInstructionType = ""; //this would ensure that if instruction
		curIsBranch = nextIsBranch; 
		nextIsBranch = 0;
		curWhichLatch = nextWhichLatch;
		nextWhichLatch = vector<int>(3,0);
		// type does not get updated, then we won't run any new commands
	}

};
struct ID
{
	public:
	bool isWorking = true;
	bool isStalling = false;
	MIPS_Architecture *arch;
	IFID* L2;
	IDEX *L3;
	string r[3] = {""}; //r[0] is the written to register, r[1] and r[2] are the using registers, they can be null
	vector<int>dataValues = vector<int>(3,0); //to be passed onto the EX stage for computation. only 2 will be sent most of the time, but for sw instruction, the value of the register would be sent as the index=2 element
	string instructionType = "";
	string addr;
	vector<string> curCommand;
	int checkforPC;
	ID(MIPS_Architecture *architecture, IFID *ifid, IDEX *idex)
	{
		arch = architecture;
		L2 = ifid;
		L3 = idex;
	}
	bool calculateLatch(string reg, int &nextWhichLatch)
	{
		if(DataHazards[reg].first >= 5)
		{
			nextWhichLatch = 0; //as this will also be computed right here, no stalls required or forwarding
		}
		else if(DataHazards[reg].second == 1)
		{
			if(DataHazards[reg].first == 3)
			{
				//then we need to stall. 
				if(arch->outputFormat == 0) cout << "stalling because I-R dependency";
				stall();
				return true;
			}
			//else we do not need to stall it. where to take the values from
			nextWhichLatch = 5; //else it can only be 5.
			if(arch->outputFormat == 0)
				cout << "DataHazard detected for " << reg << " at " << DataHazards[reg].first << endl;
		}
		else
		{
			nextWhichLatch = DataHazards[reg].first + 1; //this will be either 4 or 5
			if(arch->outputFormat == 0)
				cout << "DataHazard detected for " << reg << " at " << DataHazards[reg].first << endl;
		}
		return false;
	}
	void stall()
	{
		if(arch->outputFormat == 0)
			cout << "**";
		isStalling = true;	//then we should stall this stage right now.
		L2->IDisStalling = true;
		L3->nextInstructionType = ""; //sending null as instruction
		return;				//in the stall stage, we will not do any updated to the L3 latch, 
							//so the next values for the next stage will be the default blanks	
	}
	void run()
	{
		if(!isStalling) //if it is stalling, then we do not update the current command and isWorking status
		{
			curCommand = L2->currentCommand; //we get the command from the L2 flipflop between IF and ID
			isWorking = L2->curIsWorking; 
			checkforPC = L2->PC; 
			L3->PCrun = checkforPC;
		}
		if(isWorking == false)
		{
			L3->nextInstructionType = "";
			L3->nextIsWorking = false;
		}
		//on the basis of the commands we got, we can assign further
		if(arch->outputFormat == 0)
			cout << " |ID|=> ";
		
		if(curCommand.size() == 0)
			return;
		else if(curCommand[0] == "afterJump") //if the previous instruction was a jump, it will have been calculated by now so we can stop the stalling
		{
			L2->IDisStalling = false;
			isStalling = false; 
			return;
		}
		else if(curCommand[0] == "afterBranch")
		{
			stall();
			curCommand[0] = "afterJump"; //so that the next time we can stop the stalling
			return;
		}
		else if(curCommand[0] == "")
			return;
		instructionType = curCommand[0];
		for (int i = 1; i < 4 && i < curCommand.size(); i++)
		{
			r[i-1] = curCommand[i];
		}
		int curInstruction = arch->instructionNumber(instructionType);
		if(instructionType == "j") //then its a jump instruction, in which case we should jump to the address label, using the label
		{
			if(arch->outputFormat == 0)
				cout << "jumped to instruction number " << arch->address[curCommand[1]];
			arch->j(curCommand[1],"", ""); //and then we must introduce a stall after this stage so as to 
			//not let a wrong instruction go by.
			//the above code ensures that arch->PCnext has been updated correctly.
			if(arch->outputFormat == 0)
				cout << "PC= " << checkforPC;
			curCommand[0] = "afterJump";
			stall();
			if(arch->address[curCommand[1]] >= arch->commands.size())
			{
				L3->nextInstructionType = "";
				L3->nextIsWorking = false; //then we need to stop the execution here
			}
			return;
		}

		if(instructionType == "beq" || instructionType == "bne") //doing the entire BEQ and BNE process in ID step itself, while introducing a bubble in the pipeline where nothing gets done
		{
			if(instructionType == "beq")
				L3->nextIsBranch = 1;
			else
				L3->nextIsBranch = 2;
			dataValues[0] = arch->registers[arch->registerMap[r[0]]];
			dataValues[1] = arch->registers[arch->registerMap[r[1]]];
			if(!DataHazards.count(r[0]))
			{
				L3->nextWhichLatch[0] = 0; //because the value is generated here
			}
			else
			{
				//else its a dataHazard, there may be a stall required if the instruction was of I type and just before this one
				if(calculateLatch(r[0], L3->nextWhichLatch[0])) //returns true if we are stalling and should return
					return;
			}
			if(!DataHazards.count(r[1]))
			{
				L3->nextWhichLatch[1] = 0; //because the value is generated here
			}
			else
			{
				//else its a dataHazard, there may be a stall required if the instruction was of I type and just before this one
				if(calculateLatch(r[1], L3->nextWhichLatch[1])) //returns true if we are stalling and should return
					return;
			}
			dataValues[2] = arch->address[r[2]]; //the address can be decoded rightaway as it is static
			r[0] = r[2]; //but we are still passing it as a string through this
			bool isEqual = (dataValues[0] == dataValues[1]);
			curCommand[0] = "afterBranch";
			stall();
			UpdateL3();
			// 	L3->nextInstructionType = "branchEnd"; //incase the branch goes out of bounds, will handle in EX itself.
			// L3->nextInstructionType = instructionType;
			return;
		}
		else if(curInstruction == 2)
		{
			//this is when we are in lw and sw, in this case for the address variable,
			//a similar procedure would be done as in the case of R type registers
			pair<int,string> res = arch->decodeAddress(r[1]);    // we implemented a new function decodeAddress which correctly decodes as string and int 
			dataValues[0] = res.first;
			dataValues[1] = arch->registers[arch->registerMap[res.second]];	
			dataValues[2] = arch->registers[arch->registerMap[r[0]]]; //here the sw thing happens
			if(instructionType == "sw"){
				if(arch->outputFormat == 0)
					cout << " passed " << dataValues[2] << " for sw ";	
			}
			if(instructionType == "lw"){
				L3->secondregister = res.second;
				if(arch->outputFormat == 0)
					cout << "Passed" << " "<< dataValues[1]<<"for lw";
			}	
			
			//for dataValues[1] we might need to forward to ex for address calculation.
			if(!DataHazards.count(res.second))
			{
				L3->nextWhichLatch[1] = 0; //because the value is generated here
			}
			else
			{
				//else its a dataHazard, there may be a stall required if the instruction was of I type and just before this one
				if(calculateLatch(res.second, L3->nextWhichLatch[1])) //returns true if we are stalling and should return
					return;
			}
			//the above handles the address calculation part. now for the case where sw is used with
			//a dataHazard being stored into memory.
			if(instructionType == "sw")
			{
				//then the first register may also be a dataHazard, right.
				L3->nextWhichLatch[2] = 0; 
				if(!DataHazards.count(r[0]))
				{
					//because the value is generated here
				}
				else if(DataHazards[r[0]].first < 5)
				{
					//else its a dataHazard, there may be a stall required if the instruction was of I type and just before this one
					L3->nextWhichLatch[2] = DataHazards[r[0]].first; //this can either be 3 or 4
					//if this is 3, then we can take the value from L5 when we reach DM.
					//if this is 4, then we can take the value from L5 when we reach EX.
				}
			}
		}
		else if(curInstruction == 0)
		{
			dataValues[0] = arch->registers[arch->registerMap[r[1]]];
			dataValues[1] = arch->registers[arch->registerMap[r[2]]];
			if(!DataHazards.count(r[1]))
			{
				L3->nextWhichLatch[0] = 0; //because the value is generated here
			}
			else
			{
				//else its a dataHazard, there may be a stall required if the instruction was of I type and just before this one
				if(calculateLatch(r[1], L3->nextWhichLatch[0])) //returns true if we are stalling and should return
					return;
			}

			if(!DataHazards.count(r[2]))
			{
				L3->nextWhichLatch[1] = 0; //because the value is generated here
			}
			else
			{
				//else its a dataHazard, there may be a stall required if the instruction was of I type and just before this one
				if(calculateLatch(r[2], L3->nextWhichLatch[1])) //returns true if we are stalling and should return
					return;
			}
		}
		else if(curInstruction == 1)
		{
			//in this case, we have that
			dataValues[0] = arch->registers[arch->registerMap[r[1]]];
			dataValues[1] = stoi(r[2]);
			L3->nextWhichLatch[1] = 0; //because the value is generated here
			if(!DataHazards.count(r[1]))
			{
				L3->nextWhichLatch[0] = 0; //because the value is generated here
			}
			else
			{
				//else its a dataHazard, there may be a stall required if the instruction was of I type and just before this one
				if(calculateLatch(r[1], L3->nextWhichLatch[0])) //returns true if we are stalling and should return
					return;
			}
		}

		
		if(arch->outputFormat == 0)
			cout << " decoded " << instructionType << " ";
		if(instructionType != "sw" && instructionType != "beq" && instructionType != "bne" && instructionType != "j")
		{
			DataHazards[r[0]].first = 2;
			DataHazards[r[0]].second = (instructionType=="lw")? 1 : 0;
		}
		L2->IDisStalling = false;
		isStalling = false; 
	
		if(!isStalling)
			if(arch->outputFormat == 0)
				cout << dataValues[0] << " and " << dataValues[1] << " "<<"PC="<< checkforPC;
		UpdateL3();
	}

	void UpdateL3()
	{
		//on getting the updated values, we can run the code
		L3->nextData = dataValues;
		L3->nextInstructionType = instructionType;
		L3->nextWriteReg = r[0];
	}
};
struct DMWB{
		string currRegister = "";
		string nextRegister = "";
		int curr_data;
		int next_data;
		bool curIsWorking = true, nextIsWorking = true;
		int PC;
		int PCrun;
		void Update(){
				currRegister = nextRegister;    //on getting the updated value we can run the code
				nextRegister = "";
				curr_data = next_data;
				PC = PCrun;
				next_data = 0; curIsWorking = nextIsWorking;
		}
};
struct EXDM //the latch between EX and DM L4
{
	string curReg, nextReg;
	int curSWdata, nextSWdata;
	int curDataIn, nextDataIn;
	int curMemWrite, nextMemWrite = 0;
	bool curIsWorking = true, nextIsWorking = true;
	int PC; int PCrun;
	vector<int> curWhichLatch = vector<int>(3,0), nextWhichLatch = vector<int>(3,0);

	void Update()
	{
		// curAddr = nextAddr; curReg = nextReg;
		curMemWrite = nextMemWrite; curDataIn = nextDataIn;
		curReg = nextReg; curIsWorking = nextIsWorking;
		PC = PCrun;
		nextMemWrite = -1; curSWdata = nextSWdata;
		nextReg = "";
		curWhichLatch = nextWhichLatch; 
		nextWhichLatch = vector<int>(3,0);
		
	}
};
struct EX
{	
	public:
	bool isWorking = true; int swData;
	MIPS_Architecture *arch; IDEX *L3; EXDM *L4; DMWB *L5; //The L3 and L4 Latchs
	string iType = "";
	vector<int> dataValues; 
	int result = 0;
	string r1; //register to be written into, this will not be used in this step but passed forward till the WriteBack stage where it will be written into
	//now we decode the instruction from the instructions map
	int checkforPC;
	EX(MIPS_Architecture *architecture, IDEX *l3, EXDM *l4, DMWB *l5)
	{
		arch = architecture; L3 = l3; L4 = l4; L5 = l5;//the latch reference and architecture reference is stored at initialization
		dataValues = vector<int>(3,0);
	}

	void run()
	{	
		if(iType != "afterBranchEnd")
			iType = L3->curInstructionType; 
		isWorking = L3->curIsWorking; 
		if(arch->outputFormat == 0)
			cout << " |EX|=> ";
		checkforPC = L3->PC;
		L4->PCrun = checkforPC;
		if(!isWorking)
		{
			L4->nextIsWorking = false;
		}
		if(iType == "")
		{
			L4->nextReg = ""; L4->nextDataIn = -1;
			return;
		}
		if(iType == "branchEnd")
		{
			L4->nextReg = ""; L4->nextDataIn = -1;
			L3->nextIsWorking = false;
			if(arch->outputFormat == 0)
				cout << "BranchEnd";
			return;
		}
		
		dataValues = L3->curData; 
		if(L3->curWhichLatch[0] > 0)
		{
			dataValues[0] = (L3->curWhichLatch[0] == 4)? L4->curDataIn : L5->curr_data;
		}
		if(L3->curWhichLatch[1] > 0)
		{
			dataValues[1] = (L3->curWhichLatch[1] == 4)? L4->curDataIn : L5->curr_data;
		}
		if(L3->curWhichLatch[2] == 4)
		{
			dataValues[2] = L5->curr_data;
		}
		r1 = L3->curWriteReg; 
		if(L3->curIsBranch > 0)
		{
			//then we are in a branch instruction
			bool isEqual = (dataValues[0] == dataValues[1]);
			if(isEqual^(L3->curIsBranch == 2))
			{
				//then we need to jump to the address
				//we need to update the PC
				if(arch->outputFormat == 0)
					cout << "branched to instruction number " << r1 << ":" << arch->address[r1];
				arch->j(r1,"","");
			}
			else
			{
				if(arch->outputFormat == 0)
					cout << "did not branch ";
			}
			if(arch->PCnext >= arch->commands.size())
			{
				L4->nextReg = ""; L4->nextDataIn = -1;
				L3->nextIsWorking = false;
				return;
			}
			return;
		}
		result = calc(); 
		L4->nextWhichLatch = L3->curWhichLatch;
		if(iType == "sw")
		{
			L4->nextSWdata = dataValues[2];
		}
		// if(iType == "lw"){
		// 	dataValues[1] = arch->registers[arch->registerMap[L3->secondregister]]; //suspect
		// }
		L4->nextReg = r1; L4->nextDataIn = result; 
		L4->nextMemWrite = (iType == "sw")? 1 : (iType == "lw") ? 0 : -1;
		if(iType!="sw" && iType !="lw"){
			if(arch->outputFormat == 0)
				cout << " did " << iType << " " << dataValues[0] << " " << dataValues[1] << " "<<"PC "<<checkforPC;}
		else{
			if(arch->outputFormat == 0)
				cout<<"address"<<dataValues[0] << " + " << dataValues[1] << "calculated"; 
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
struct DM
{
	public:
	bool isWorking = true;
	MIPS_Architecture *arch; EXDM *L4; DMWB *L5; //the references to architecture and the L4 Latch 
	string reg; 
	int checkforPc;
	int memWrite;  //when memWrite is 1, then we write from register into the memory
					//when memWrite is 0. then we read from memory into register, so it is passed onto the WB stage to do that
					//when it is -1, then we skip this stage and move onto the Writeback stage
	int swData;
	int dataIn; 	//dataIn is an address if memWrite is 1 or 0, otherwise its value will be directly stored onto the register in WB stage
	DM(MIPS_Architecture *architecture, EXDM *exdm, DMWB *dmwb)
	{
		arch = architecture; L4 = exdm; L5 = dmwb; //initialisation
	}

	void run()
	{
		isWorking = L4->curIsWorking;
		checkforPc = L4->PC;
		L5->PCrun = checkforPc;
		if(arch->outputFormat == 0)
			cout << " |DM|=> "; 
		if(!isWorking)
		{
			L5->nextIsWorking = false;
			return;
		}
		reg = L4->curReg; memWrite = L4->curMemWrite; dataIn = L4->curDataIn;
		swData = L4->curSWdata;
		//updated all the values using the latch L4
		if(L4->curWhichLatch[2] == 3)
		{
			// if(arch->outputFormat == 0)
			// 	cout << "forwarded from L5";
			swData = L5->curr_data; //forwarding the value from the L5 latch
		}
		if(reg == "")
		{
			L5->nextRegister = "";
			return; //nothing to do here
		}

		if(memWrite == 1)
		{
			//then we write into the memory
			if(dataIn%4 != 0) 
			{
				cerr << endl << "<!---Error: Address not word aligned at PC= " << checkforPc << "---!>" << endl;
				return;
			}
			arch->data[dataIn/4] = swData; //storing into the register what we decoded from a register file back in the ID stage
			if(arch->outputFormat == 0)	
				cout << " sent val " << swData << " into memory at " << dataIn<< "PC="<<checkforPc;
			L5->next_data = -1; L5->nextRegister = ""; //since we dont need to write anything onto the register, the reg is passed as ""
		}
		else
		{
			//we must read from the memory into the register, so
			if(memWrite == 0)
			{
				if(dataIn%4 != 0) 
				{
					cerr << endl << "<!---Error: Address not word aligned at PC= " << checkforPc << "---!>" << endl;
					return;
				}
				dataIn = arch->data[dataIn/4]; 
				if(arch->outputFormat == 0)
					cout<< "sending value" << " " <<dataIn <<" "<<"from Memory to  register" <<" "<<reg<<" "<<"PC="<<checkforPc; 
			}
			//if memWrite is instead -1, then we simply pass on the value of dataIn directly.
			L5->nextRegister = reg;
			L5->next_data = dataIn; 
		}
	}

};
struct WB{
	public:
	bool isWorking = true;
	string r2;
	int checkForPC;
	int new_data;
	MIPS_Architecture *arch;
	DMWB* L5;
	WB(MIPS_Architecture *a,DMWB *dmwb){
		arch = a;
		L5 = dmwb;
	}
	void run(){
		isWorking = L5->curIsWorking;
		r2 = L5->currRegister;
		new_data  = L5->curr_data;
		checkForPC = L5->PC;
		if(arch->outputFormat == 0)
			cout << " |WB|=> ";
		if(r2 != "")
		{
			arch->registers[arch->registerMap[r2]] = new_data;
			if(arch->outputFormat == 0)
				cout << "wrote " << new_data << " into reg " << r2 << " "<<"currPC"<<L5->PC;
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
		IFID L2; //The Latches
		IDEX L3;
		EXDM L4;
		DMWB L5;
		IF fetch(arch, &L2); //fetch is the IF stage
		ID Decode(arch,&L2, &L3); //Decode is the ID stage
		EX ALU(arch, &L3, &L4, &L5); //all are self explanatory actually
		DM DataMemory(arch,&L4,&L5);
		WB WriteBack(arch,&L5);
		arch->printRegisters(clockCycles);
		cout << 0 << endl;
		while(DataMemory.isWorking)
		{
			WriteBack.run(); //First half Cycle
			Decode.run(); //Second Half Cycle, Decode running before IF so it can detect stalls and make IF stall
			fetch.run();
			ALU.run();
			DataMemory.run();
			
			L2.Update(); L3.Update(); L4.Update(); L5.Update(); //updated the intermittent latches
			clockCycles++;
			arch->printRegisters(clockCycles);
			if(DataMemory.memWrite == 1)
			{
				std::cout << 1 << " " << DataMemory.dataIn/4 << " " << DataMemory.swData;
			}
			else
			{
				std::cout << 0;
			}
			if(arch->outputFormat == 0) 
			{	
				std::cout << " dataHazards are : ";
				for(auto i: DataHazards)
				{	
					std::cout << i.first << " " << i.second.first << ":" << i.second.second <<", ";
				}
			}
			
			HazardUpdate(6); //updating the hazards

			//cout << endl << " at clockCycles " << clockCycles << endl;
			std::cout << endl;
		}
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

