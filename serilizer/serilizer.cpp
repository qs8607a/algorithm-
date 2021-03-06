#include <iostream>
#include <fstream>
#include <cstring>
#include "serilizer.h"

using namespace std;

serial_node::~serial_node(){
	if(s)
		delete[] s;
}

serial_class::serial_class(int length){
	serial_array[length];
	for(int i = 0; i < length; i++){
		serial_array[i].a = 67 + i;
	    serial_array[i].b = i;
		const char *s_tmp = "abc";
		serial_array[i].s = new char[strlen(s_tmp)];
		serial_array[i].s_len = strlen(s_tmp);
		strcpy(serial_array[i].s,s_tmp);
	}
}

bool serial_class::serialize(string path){
	ofstream ofs;
	ofs.open(path.c_str(),ios::out|ios::binary|ios::app);
	if(ofs.good()){
		int length_tmp = serial_class::length;
		ofs.write((char *)&(length_tmp),sizeof(int));
		ofs.write((char *)this->serial_array,sizeof(serial_node)*(this->length));	
		for(int i = 0; i < length; i++){
			ofs.write((char *)((this->serial_array + i)->s),(this->serial_array + i)->s_len);
		}
		ofs.close();
		return true;
	}	
	ofs.close();
	return false;
}

serial_node* serial_class::unserialize(string path){
	ifstream ifs;
	ifs.open(path.c_str(),ios::in|ios::binary);
	int read_len = -1;
	ifs.read((char *)&read_len,sizeof(int));
	if(read_len <= 0){
		cout << "read file error" << endl;
	    return NULL;	
	}
	serial_node *serial_obj = new serial_node[length];
	ifs.read((char *)serial_obj,sizeof(serial_node)*read_len);
	for(int i = 0; i < length; i++){
		serial_node *tmp_obj = serial_obj + i;
		if(tmp_obj&&tmp_obj->s_len >= 0){
			tmp_obj->s = new char[tmp_obj->s_len + 1];
			ifs.read(tmp_obj->s,sizeof(tmp_obj->s_len));
			tmp_obj->s[tmp_obj->s_len] = '\0';
		}
	}
	ifs.close();
	return serial_obj;
}

int main(){
	serial_class serial_obj(5);
	bool serial_res = serial_obj.serialize("obj.serial");
	if(serial_res)
		cout << "serial success~" << endl; 
	else
		cout << "serial bad!~" << endl;
	serial_node *res_obj = serial_obj.unserialize("obj.serial");
	if(serial_res){
		cout << res_obj->s << endl;
	}
	return 0;
}
