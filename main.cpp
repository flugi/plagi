#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <set>
#include <sys/time.h>

using namespace std;

int verbose = 1;


inline int charcount(const string &s, char c) {
    int sum=0;
    for (char cc:s) {
        sum+=(cc==c);
    }
    return sum;
}

inline string trim(string s) {
    if (s.length()==0) return s;
    int p=0;
    while (s[p]==' ' || s[p]=='\t') p++;
    int e = s.length()-1;
    while (s[e]==' ' || s[e]=='\t') e--;
    s=s.substr(p,e-p+1);
    return s;
}

inline string compact(string s) {
    string res="";
    for (char c:s) {
        if (c!=' ') res+=c;
    }
    return res;
}

inline void replace(string &s, char from, char to) {
    for (char &c: s) {
        if (c==from) c=to;
    }
}

float salientscore(string s, int hitcount) {
    if (hitcount==0) return 0;
    float cplx = 1+3*charcount(s,',')+charcount(s,'(')+charcount(s,'|')+charcount(s,'+')+charcount(s,'-')+charcount(s,'&');
    float lengthsc = s.length()/10.0;
    return lengthsc*cplx/hitcount;
}

const set<string> keywords({"for", "while","if","float","int","return"});

string skeleton(string s) {
    string res;
    string delim = " ,.()[]{}+-*/&|%^!~:?<>=;";
    string wrd = "";
    for (int i=0;i<s.length();i++) {
        if (delim.find(s[i])!=string::npos) {
            if (wrd!="") {
                if (keywords.find(wrd)==keywords.end()) res+="id";
                    else res+=wrd;
            }
            res+=s[i];
            wrd="";
        } else {
            wrd+=s[i];
        }
    }
    return res;
}

struct Benchmark {
    int64_t _start;
    string _s;
    Benchmark(string s) : _s(s) {
        timeval tv;
        gettimeofday(&tv,0);
        _start = tv.tv_sec*1000000+tv.tv_usec;
    }
    ~Benchmark()  {
        timeval tv;
        gettimeofday(&tv,0);
        int64_t end = tv.tv_sec*1000000+tv.tv_usec;
        cerr << "BENCHMARK: " << _s << " " << end-_start << endl;
    }

};

class File {
    vector<string> _content;
    string _name;
    string _author;
    set<string> _s;
public:
    File(string fname) {
        ifstream f(fname);
        if (!f.good()) {
            cerr << "cannot open file `" << fname << "`" << endl;
            throw std::runtime_error("cannot open file");
        }
        _name = fname;
        string line;
        getline(f, line);
        while (f.good()) {
            _content.push_back(line);
            _s.insert(skeleton(trim(line)));
            getline(f, line);
        }
        stringstream ss;
        ss << _name << endl;
        string kuka;
        getline(ss,kuka,'/');
        getline(ss,_author, '_');
    }
    string author() const {return _author;}
    string name() const {return _name;}
    const set<string>& getset() const {return _s;}

    void check(const File & m) {
        int sum=0;
        vector<string> vs;
        for (string s : _content) {
            if (trim(s)=="{" || trim(s)=="}" || trim(s)=="else" || trim(s)=="break;") continue;

            for (string sk : m._content) {
                if (s==sk) {
                    sum+=s.length();
                    vs.push_back(s);
                }
            }
        }
        if (sum>300 && _author != m._author) {

//            cout <<sum << " " << _name << "|" << m._name << endl;
            string outfilename = _name+string("_")+m._name+".common";
            replace(outfilename, '/','_');
            ofstream f(outfilename);
            for (string s: vs) {
                f<< s << endl;
            }
            f.close();
        }
    }
};

class Config {
    vector<File*> _files;
public:
    Config (string filelist) {
        ifstream f(filelist);
        if (!f.good()) {
            cerr << "cannot open file `" << filelist << "`" << endl;
            throw std::runtime_error("cannot open file");
        }
        string fname;
        getline(f,fname);
        while(f.good()) {
            File * file = new File(fname);
            _files.push_back(file);
            getline(f,fname);
        }
        cout << "loaded " << _files.size() << " files" << endl;
    }
    ~Config() {
        for (File* fp : _files) {
            delete fp;
        }
    }
    void check() {
        int c=_files.size();
        int done=0;
#pragma omp parallel
#pragma omp for schedule(dynamic,10)
        for (int i=0;i<c;i++) {
#pragma omp critical
            done++;
            if (verbose) cerr <<"\r"<< 100*done/c << "%             " ;
            //Benchmark b(_files[i]->name());
            for (int j=i+1;j<c;j++) {
                _files[i]->check(*_files[j]);
            }
        }
    }
    void salient() {
        ofstream of("salient.txt");
        int c=_files.size();
        for (int i=0;i<c;i++) {
            if (verbose) cerr <<"\r"<< 100*i/c << "%      " ;
            const set<string> & a = _files[i]->getset();
            for (string s : a) {
                int hit = 0;
                set<string> others;
                others.insert(_files[i]->author());
                for (int j=0;j<c;j++) {
                    if (_files[i]->author() != _files[j]->author()){
                        const set<string> & b = _files[j]->getset();
                        if (b.find(s)!=b.end()) {
                            hit++;
                            others.insert(_files[j]->author());
                        }
                    }
                }
                string sources="";
                for (string s : others) {
                    sources+=s+" ";
                }
                if (hit>0 && others.size()<10) {
                    of <<int(1000*salientscore(s,others.size())) << " `" << sources << "` "<< " " << s << "`" <<_files[i]->name() << endl;
                }
            }
        }
        of.close();
    }
};

int main(int argc, char* argv[]) {
    stringstream ss;
    for (int i=1; i<argc; i++) {
        ss << argv[i] << " ";
    }
    string filelist;
    ss >> filelist;
    if (!ss.good()) {
        cerr << "Usage: " << argv[0] << " filelist.txt" << endl;
        exit(1);
    }
    Config config(filelist);
    config.salient();
    config.check();
}
