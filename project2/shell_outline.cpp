#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <signal.h>
#include <sstream>                                  // for istringstream
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/errno.h>
#include <sys/param.h>                              // for MAXPATHLEN
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <readline/readline.h>
#include <readline/history.h>

using namespace std;

// enables/disables debugging
bool DEBUG = false;

// external reference to the execution environment
extern char **environ;

// the local environment tracker
struct environment {
    map<string, string> localvar;
};

// prototype for a command
typedef int (*command)(std::vector<std::string>);

// keeps track of legal commands
vector<string> legalCommands;

// headers for built-in commands
int com_ls(vector<string>);
int com_cd(vector<string>);
int com_echo(vector<string>);
int com_exit(vector<string>);
int com_pwd(vector<string>);

// duplicate a C-style string for Readline
// from: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
char *dupstr (const char *s) {
    char *r;
    r = (char *)malloc(strlen(s) + 1);
    strcpy(r, s);
    return r;
}

// a static variable for holding the line for readline.
// from: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
static char *rl_line_read = (char *)NULL;

// read a string from the commandline, and return a pointer to it.
// returns NULL on EOF.
// from: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
char *rl_gets(const char* prompt) {
    // If the buffer has already been allocated, then return the memory to the
    // free pool.
    if(rl_line_read) {
        free(rl_line_read);
        rl_line_read = (char *)NULL;
    }

    // Get a line from the user.
    rl_line_read = readline(prompt);

    // If the line has any text in it,
    if(rl_line_read && *rl_line_read) {
        add_history(rl_line_read);
    }

    return (rl_line_read);
}

// transform a C-style string into a C++ vector of string tokens.  The C-style
// string is tokenized on whitespace.
vector<string> tokenize(char *line) {
    vector<string> tokens;
    vector<string> tempToken;

    // Convert C-style string to C++ string object
    string lineStr(line);

    // istringstream allows us to treat the string like a stream
    istringstream ist(lineStr);
    string tokenStr;
    string tempString="";

    while (ist >> tokenStr) {
        tokens.push_back(tokenStr);
    }

    // count the number of quotation marks, which are disallowed
    for(unsigned int p = 0; p < tokens.size(); p++)
    {
        if(tokens[p].find('"') != string::npos ||
                tokens[p].find('`') != string::npos ||
                tokens[p].find('\'') != string::npos) {
            cout << "double quotes, single quotes and backticks not allowed."
                 << endl;
            tokens.clear();
        }
    }

    return tokens;
}

// generates environment variables for readline completion
char *environment_generator (const char *text, int state) {
    // convert the entered text to a C++ string
    string textStr(text);

    if(DEBUG) {
        cout << endl << "textStr = " << textStr << endl;
        cout << endl << "rl_line_buffer = " << rl_line_buffer << endl;
    }

    // this is where we hold all the matches
    // must be static because this function is called repeatedly
    static vector<string> matches;

    // if this is the first time called, initialize the index and build the
    // vector of all possible matches
    if(state == 0) {
        char* variable = environ[0];
        for(int i = 0; variable != (char *)NULL; variable = environ[++i]) {
            if(DEBUG) {
                cout << "environ[" << i << "] = " << environ[i] << endl;
            }
            string variableStr(variable);
            variableStr = "$" + variableStr.substr(0, variableStr.find("="));
            if(variableStr.find(textStr) == 0) {
                if(DEBUG) {
                    cout <<  endl << "variableStr = " << variableStr << endl;
                }
                matches.push_back(variableStr);
            }
        }
    }

    // return one of the matches, or NULL if there are no more
    if(matches.size() > 0) {
        const char *match = matches.back().c_str();

        // delete the last element
        matches.pop_back();

        // return a copy of the match
        return dupstr(match);
    }
    else {
        return ((char *)NULL);
    }
}

// generates directories for readline completion
char *directory_generator (const char *text, int state) {
    // convert the entered text to a C++ string
    string textStr(text);

    if(DEBUG) {
        cout << endl << "textStr = " << textStr << endl;
        cout << endl << "rl_line_buffer = " << rl_line_buffer << endl;
    }

    // this is where we hold all the matches
    // must be static because this function is called repeatedly
    static vector<string> matches;

    // if this is the first time called, init the index and build the
    // vector of all possible matches
    if(state == 0) {
        char path[MAXPATHLEN];
        getcwd(path, MAXPATHLEN);
        DIR *dirp = opendir(path);
        struct dirent *dp;

        while((dp = readdir(dirp)) != NULL) {
            if(dp->d_type == DT_DIR) {
                string dirStr(dp->d_name);
                if(DEBUG) {
                    cout << endl << "dp->d_name = " << dp->d_name << endl;
                }
                if(dirStr.find(textStr) == 0) {
                    if(DEBUG) {
                        cout <<  endl << "dirStr = " << dirStr << endl;
                    }
                    matches.push_back(dirStr + "/");
                }
            }
        }
        closedir(dirp);
    }

    // return one of the matches, or NULL if there are no more
    if(matches.size() > 0) {
        const char * match = matches.back().c_str();

        // delete the last element
        matches.pop_back();

        // return a copy of the match
        return dupstr(match);
    }
    else {
        return ((char *)NULL);
    }
}

// generates commands for readline completion
char *command_generator (const char *text, int state) {
    // convert the entered text to a C++ string
    string textStr(text);

    // this is where we hold all the matches
    // must be static because this function is called repeatedly
    static vector<string> matches;

    // if this is the first time called, init the index and build the
    // vector of all possible matches
    if(state == 0) {
        for(unsigned int i = 0; i < legalCommands.size(); i++) {
            // if the text entered matches one of our commands
            if(legalCommands[i].find(textStr) == 0) {
                matches.push_back(legalCommands[i]);
            }
        }

        const char * one_path = "/usr/local/bin";
        DIR *dirp = opendir(one_path);
        struct dirent *dp;

        while ((dp = readdir(dirp)) != NULL) {
            string dirStr(dp->d_name);
            if(dirStr.find(textStr) == 0) {
                matches.push_back(dirStr);
            }
        }
    }

    // return one of the matches, or NULL if there are no more
    if(matches.size() > 0) {
        const char * match = matches.back().c_str();
        // delete the last element
        matches.pop_back();
        return dupstr(match);
    }
    else {
        return ((char *)NULL);
    }
}

// try to complete with a command, directory, or variable name; otherwise, use
// filename completion
// adapted from: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
char **command_completion (const char *text, int start, int end) {
    char **matches;

    // the whole line entered so far
    string lineStr(rl_line_buffer);

    if(DEBUG) {
        cout << endl
             << "text = " << text
             << " start = " << start
             << " end = " << end << endl;
    }

    matches = (char **)NULL;

    // if this word is the start of the line, then it is a command to complete
    if(start == 0) {
        rl_completion_append_character = ' ';
        matches = rl_completion_matches(text, command_generator);
    }
    else if(lineStr.find("cd ") == 0) {
        rl_completion_append_character = '\0';
        matches = rl_completion_matches(text, directory_generator);
    }
    else if(lineStr.find("echo $") == 0) {
        rl_completion_append_character = ' ';
        matches = rl_completion_matches(text, environment_generator);
    }
    return matches;
}

// adapted from: http://cnswww.cns.cwru.edu/~chet/readline/readline.html#SEC23
void initialize_readline() {
    // setup our legal commands
    legalCommands.push_back("cd");
    legalCommands.push_back("ls");
    legalCommands.push_back("echo");
    legalCommands.push_back("exit");
    legalCommands.push_back("pwd");

    // took out the '$' out of the default so that it is part of a word
    rl_basic_word_break_characters = " \t\n\"\\'`@><=;|&{(";

    // tell the completer that we want to try completion first
    rl_attempted_completion_function = command_completion;
}

// built-in ls command - list the current directory's contents
int com_ls(vector<string> tokens) {
    // if no directory is given, use the local directory
    if(tokens.size() < 2) {
        tokens.push_back(".");
    }

    // open the directory
    DIR *dir = opendir(tokens[1].c_str());

    // catch an errors opening the directory
    if(!dir) {
        // print the error from the last system call with the given prefix
        perror("ls error: ");

        // return error
        return 1;
    }

    // process each entry in the directory
    for(dirent *current = readdir(dir); current; current = readdir(dir)) {
        cout << current->d_name << endl;
    }

    // return success
    return 0;
}

// built-in cd command - changes directory to the specified directory
int com_cd(vector<string> tokens) {
    if(chdir(tokens[1].c_str())==-1) {
	perror("cd error: ");
	return -1;
    }
    return 0;
}

// returns a string containing the current directory path
string pwd() {
    return string(getcwd(NULL,0));
}

// built-in pwd command - prints the current working directory
int com_pwd(vector<string> tokens) {
    // TODO: YOUR CODE GOES HERE
    // HINT: you should implement the actual fetching of the current directory in
    // pwd(), since this information is also used for your prompt

    cout << pwd() << endl;

    return 0;
}

// built-in echo command - prints out any given tokens
int com_echo(vector<string> tokens) {
    // TODO: YOUR CODE GOES HERE
    for(int i=1; i < tokens.size(); i++) {
	cout << tokens[i] << " ";
    }
    cout << endl;
    return 0;
}

// built-in exit command - exits
int com_exit(vector<string> tokens) {
    // TODO: YOUR CODE GOES HERE
    exit(0);
    return 0;
}

// handles external commands, redirects, and pipes
int dispatchCmd(vector<string> tokens) {
    // TODO: YOUR CODE GOES HERE
    return 0;
}

// executes a line of input by either calling dispatchCmd or executing the
// built-in command
int executeLine(vector<string> tokens,
                map<string, command> functions,
                environment& env) {
    if(tokens.size() != 0) {
        int returnValue=0;

        map<string,command>::iterator lookup = functions.find(tokens[0]);
        if(lookup == functions.end()) {
            returnValue = dispatchCmd(tokens);
        }
        else {
            returnValue = ((*lookup->second)(tokens));
        }

        return returnValue;
    }
    else {
        return 0;
    }
}

// returns a C string containing the prompt to show to the user
// does the checking of the return value to update the emoticon
// also grabs the current working directory
char* prompt(int returnValue) {
    // TODO: YOUR CODE GOES HERE (replace the following line)
    string prompt="prompt$ ";

    return rl_gets(prompt.c_str());
}

// performs variable substitution using environment variables
void varSub(vector<string>& tokens, environment& env) {
    if(tokens.size() != 0){
        for(unsigned int i = 0; i < tokens.size(); i++)
        {
            if(tokens[i].at(0) == '$')
            {
                if(getenv(tokens[i].substr(1).c_str()) != NULL) {
                    tokens[i] = getenv(tokens[i].substr(1).c_str());
                }
                else if(env.localvar.find(tokens[i].substr(1)) != env.localvar.end()) {
                    tokens[i] = env.localvar.find(tokens[i].substr(1))->second;
                }
                else {
                    tokens[i] = "";
                }
            }
        }
    }
}

// handles two things: "VAR=value" and then "VAR=value <command>"
void localVarHandling(vector<string>& tokens, environment& env) {
    if(tokens.size() != 0) {
        if((tokens.size() == 1) && (tokens[0].find("=") != string::npos)) {
            // local var assingment
            string::size_type equals = tokens[0].find("=");
            string var = tokens[0].substr(0, equals);
            string value = tokens[0].substr(equals+1);
            env.localvar[var] = value;
        }
        else if((tokens.size() >= 2) && (tokens[0].find('=') != string::npos)) {
            // local var assignment + command
            string::size_type equals = tokens[0].find('=');
            string var = tokens[0].substr(0, equals);
            string value = tokens[0].substr(equals+1);
            env.localvar[var] = value;
            tokens.erase(tokens.begin());
        }
    }
}

// the main program
int main() {
    // the map of available functions
    map<string, command> functions;

    // a vector of parsed tokens
    vector<string> tokens;

    // the shell-local environment
    environment env;

    // the raw raw command-line read from the terminal
    char* line;

    // the return value of the last command executed
    int returnValue=0;

    // register built-in commands
    functions["echo"] = &com_echo;
    functions["exit"] = &com_exit;
    functions["pwd"] = &com_pwd;
    functions["ls"] = &com_ls;
    functions["cd"] = &com_cd;

    // initialize the readline library for the shell's use
    initialize_readline();

    // loop to read many commands
    while(true) {
        // read the input from the user
        line = prompt(returnValue);

        // if the pointer is null, then an EOF has been received
        if(!line) {
            break;
        }

        // break the raw input line into tokens
        tokens = tokenize(line);

        // handle local variable setting in the form of "VAR=value" and
        // "VAR=value command"
        localVarHandling(tokens, env);

        // substitutes variable references in the command line
        varSub(tokens, env);

        // execute the line
        returnValue = executeLine(tokens, functions, env);
    }

    return 0;
}
