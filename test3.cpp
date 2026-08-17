#include <iostream>
#include <string>
#include <cctype>
#include <vector>

int main() {
    std::string new_type = "ptr<T>";
    std::vector<std::string> args = {"string"};
    std::vector<std::string> params = {"T"};
    
    for(size_t i=0; i<args.size(); i++){
        if(new_type==params[i]) new_type=args[i];
        else if(new_type.find(params[i]) != std::string::npos) {
            size_t pos = new_type.find(params[i]);
            while(pos != std::string::npos){
                bool before_ok = (pos==0 || !isalnum(new_type[pos-1]));
                bool after_ok = (pos+params[i].length() == new_type.length() || !isalnum(new_type[pos+params[i].length()]));
                if(before_ok && after_ok) {
                    new_type.replace(pos, params[i].length(), args[i]);
                    pos+=args[i].length();
                } else {
                    pos+=params[i].length();
                }
                pos=new_type.find(params[i], pos);
            }
        }
    }
    std::cout << "Result: " << new_type << std::endl;
}
