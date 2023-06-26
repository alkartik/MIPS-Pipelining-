#include<BranchPredictor.hpp>

int getLeast14(std::string hex)
{
    //will just check the last 4 digits of it, and then remove the 2 msbs
    int num = 0; int mult = 1;
    for (int i = hex.size()-1; i >= 4; i--)
    {
        if(hex[i] >= 'a')
            num += (hex[i]-'a'+10)*mult;
        else
            num += (hex[i] - '0')*mult;
        mult *= 16;
    }
    num = (num & 16383); //done to get only the first 14 bits. 16383 is 2^14 - 1
    return num;
}
int main()
{
    string cntOut[4] = {"bhr00.txt", "bhr01.txt", "bhr10.txt","bhr11.txt"};
    for (int i = 0; i < 4; i++)
    {
        int initialValueOfCounter = i; //makes an array of 2^14 counters with the given starting state
        BHRBranchPredictor counters = (BHRBranchPredictor(initialValueOfCounter));
        ifstream branchtrace; 
        branchtrace.open("branchtrace.txt");
        ofstream outfile(cntOut[i]);
        int correctPredictions = 0;
        int total = 0;
        outfile << "array counter VARIANT where initially all counters are in state " << i << endl;
        outfile << endl;
        if(!branchtrace.is_open())
        {
            cout << "file cannot be opened" << endl; return -1;
        }    
        while(branchtrace.good())
        {
            string a; int b;
            branchtrace >> a;
            if(branchtrace.good()) //because sometimes the >> operator duplicates the last one
            {
                total++;
                uint32_t index  = getLeast14(a);
                bool prediction = counters.predict(index);
                outfile << "Prediction for " << a << ":" << index << " is =>" << prediction << " at state " << counters.bhr[1] << counters.bhr[0] << " which is ";
                branchtrace >> b;
                if( b == prediction)
                {
                    outfile<<"b"<<" "<<prediction;
                    correctPredictions++;
                    outfile << "CORRECT" << endl; //prints correct if the prediction matches
                }
                else
                {
                    outfile<<"b"<<" "<<prediction;
                    outfile << "WRONG" << endl; //else it prints wrong if the prediction was incorrect
                }
                counters.update(index,b);
            }
        }
           
        outfile << endl;
        outfile << "correct = " << correctPredictions << " out of " << total << endl;
        outfile << " Total Accuracy => " << (double)(correctPredictions)/total << endl; //prints the accuracy as a decimal between 0 to 1
        outfile << endl;

        outfile.close();
        branchtrace.close(); 
    }



}
