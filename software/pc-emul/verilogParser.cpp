#include "verilogParser.hpp"

#include <cstdio>
//#include <vector>

#include "memory.hpp"

#include "templateEngine.hpp"

static MacroMap macros;
static Arena* tempArena;

void PerformDefineSubstitution(Arena* output,String name){
   String subs = macros[name];

   Tokenizer inside(subs,"`",{});

   while(!inside.Done()){
      Token peek = inside.PeekFindUntil("`");

      if(peek.size < 0){
         break;
      } else {
         inside.AdvancePeek(peek);
         PushString(output,peek);

         inside.AssertNextToken("`");

         Token name = inside.NextToken();

         PerformDefineSubstitution(output,name);
      }
   }

   Token finish = inside.Finish();
   PushString(output,finish);
}

void PreprocessVerilogFile_(Arena* output, String fileContent,std::vector<const char*>* includeFilepaths);

static void DoIfStatement(Arena* output,Tokenizer* tok,std::vector<const char*>* includeFilepaths){
   Token first = tok->NextToken();
   Token macroName = tok->NextToken();

   bool compareVal = false;
   if(CompareString(first,"`ifdef") || CompareString(first,"`elsif")){
      compareVal = true;
   } else if(CompareString(first,"`ifndef")){
      compareVal = false;
   } else {
      UNHANDLED_ERROR;
   }

   bool exists = (macros.find(macroName) != macros.end());
   bool doIf = (compareVal == exists);

   void* mark = tok->Mark();
   while(!tok->Done()){
      Token peek = tok->PeekToken();

      if(CompareString(peek,"`endif")){
         if(doIf){
            Token content = tok->Point(mark);
            PreprocessVerilogFile_(output,content,includeFilepaths);
         }
         tok->AdvancePeek(peek);
         break;
      }

      if(CompareString(peek,"`else")){
         if(doIf){
            Token content = tok->Point(mark);
            PreprocessVerilogFile_(output,content,includeFilepaths);
         } else {
            tok->AdvancePeek(peek);
            mark = tok->Mark();
            doIf = true;
         }
         continue;
      }

      if(CompareString(peek,"`ifdef") || CompareString(peek,"`ifndef") || CompareString(first,"`elsif")){
         DoIfStatement(output,tok,includeFilepaths);
      } else {
         tok->AdvancePeek(peek);
      }
   }
}

void PreprocessVerilogFile_(Arena* output, String fileContent,std::vector<const char*>* includeFilepaths){
   Tokenizer tokenizer = Tokenizer(fileContent, "()`\\\",+-/*",{"`include","`define","`timescale","`ifdef","`else","`elsif","`endif","`ifndef"});
   Tokenizer* tok = &tokenizer;

   while(!tok->Done()){
      Token peek = tok->PeekToken();
      if(CompareToken(peek,"`include")){ // Assuming defines only happen outside module (Not correct but follows common usage, never seen include between parameters or port definitions)
         tok->AdvancePeek(peek);
         tok->AssertNextToken("\"");

         Token fileName = tok->NextFindUntil("\"");
         tok->AssertNextToken("\"");

         // Open include file
         std::string filename(UNPACK_SS_REVERSE(fileName));
         FILE* file = nullptr;
         std::string filepath;
         for(const char* str : *includeFilepaths){
            filepath = std::string(str) + filename;

            file = fopen(filepath.c_str(),"r");

            if(file){
               break;
            }
         }

         if(!file){
            printf("Couldn't find file: %.*s\n",UNPACK_SS(fileName));
            printf("Looked on the following folders:\n");

            printf("  %s\n",GetCurrentDirectory());
            for(const char* str : *includeFilepaths){
               printf("  %s\n",str);
            }

            DEBUG_BREAK();
            exit(0);
         }

         size_t fileSize = GetFileSize(file);

         Byte* mem = PushBytes(tempArena,fileSize + 1);
         fread(mem,sizeof(char),fileSize,file);
         mem[fileSize] = '\0';

         PreprocessVerilogFile_(output,STRING((const char*) mem,fileSize),includeFilepaths);

         fclose(file);
      } else if(CompareToken(peek,"`define")){ // Same for defines
         tok->AdvancePeek(peek);
         Token defineName = tok->NextToken();

         Token emptySpace = tok->PeekWhitespace();
         if(emptySpace.size == 0){ // Function macro
            Token arguments = tok->PeekFindIncluding(")");
            defineName = ExtendToken(defineName,arguments);
            tok->AdvancePeek(arguments);
         }

         FindFirstResult search = tok->FindFirst({"\n","//","\\"});
         Token first = search.foundFirst;
         Token body = {};

         if(CompareString(first,"//")){ // If single comment newline or slash, the macro does not contain the comment
            body = tok->PeekFindUntil("//");
         } else {
            Token line = tok->PeekFindUntil("\n");
            body = line;
            while(!tok->Done()){
               if(line.size == -1){
                  line = tok->Finish();
                  body = ExtendToken(body,line);
                  break;
               }

               Tokenizer inside(line,"\\",{}); // Handles slices inside comments

               bool hasSlice = false;
               while(!inside.Done()){
                  Token t = inside.NextToken();
                  if(CompareString(t,"\\")){
                     hasSlice = true;
                     break;
                  }
               }

               if(hasSlice){
                  tok->AdvancePeek(line);
                  body = ExtendToken(body,line);
                  line = tok->PeekFindUntil("\n");
               } else {
                  tok->AdvancePeek(line);
                  body = ExtendToken(body,line);
                  break;
               }
            }
         }

         //printf("`%.*s  %.*s\n",defineName.size,defineName.str,mini(10,body.size),body.str);
         macros[defineName] = body;
      } else if(CompareToken(peek,"`timescale")){
         Token line = tok->PeekFindIncluding("\n");

         tok->AdvancePeek(line);
      } else if(CompareToken(peek,"`")){
         tok->AdvancePeek(peek);
         Token name = tok->NextToken();

         PerformDefineSubstitution(output,name);
      } else if(CompareToken(peek,"`ifdef") || CompareToken(peek,"`ifndef")){
         DoIfStatement(output,tok,includeFilepaths);
      } else if(CompareToken(peek,"`else")){
         NOT_POSSIBLE;
      } else if(CompareToken(peek,"`endif")){
         NOT_POSSIBLE;
      } else if(CompareToken(peek,"`elsif")){
         NOT_POSSIBLE;
      } else {
         tok->AdvancePeek(peek);

         PushString(output,peek);
      }

      PushString(output,tok->PeekWhitespace());
   }
}

String PreprocessVerilogFile(Arena* output, String fileContent,std::vector<const char*>* includeFilepaths,Arena* arena){
   macros.clear();
   tempArena = arena;

   String res = {};
   res.data = (const char*) &output->mem[output->used];
   Byte* mark = MarkArena(output);

   Byte* tempMark = MarkArena(tempArena);
   PreprocessVerilogFile_(output,fileContent,includeFilepaths);
   PopMark(tempArena,tempMark);

   PushString(output,STRING("\0"));
   res.size = MarkArena(output) - mark;

   #if 0
   for(auto key : macros){
      printf("%.*s\n",key.first.size,key.first.str);
   }
   #endif

   return res;
}

static Expression* ParseAtom(Tokenizer* tok){
   Expression* expr = PushStruct<Expression>(tempArena);

   Token peek = tok->PeekToken();

   if(IsNum(peek[0])){
      int res = ParseInt(peek);
      tok->AdvancePeek(peek);

      expr->val = MakeValue(res);
      expr->type = Expression::LITERAL;

      return expr;
   } else if(peek[0] == '"'){
      tok->AdvancePeek(peek);
      Token str = tok->PeekFindUntil("\"");
      tok->AdvancePeek(str);
      tok->AssertNextToken("\"");

      expr->val = MakeValue(str);
      expr->type = Expression::LITERAL;

      return expr;
   }

   Token name = tok->NextToken();

   expr->id = name;
   expr->type = Expression::IDENTIFIER;

   return expr;
}

static Expression* ParseExpression(Tokenizer* tok);

static Expression* ParseFactor(Tokenizer* tok,Arena* arena){
   Token peek = tok->PeekToken();

   if(CompareString(peek,"(")){
      tok->AdvancePeek(peek);
      Expression* expr = ParseExpression(tok);
      tok->AssertNextToken(")");
      return expr;
   } else {
      Expression* expr = ParseAtom(tok);
      return expr;
   }
}

static Value Eval(Expression* expr,ValueMap& map){
   switch(expr->type){
   case Expression::OPERATION:{
      Value val1 = Eval(expr->expressions[0],map);
      Value val2 = Eval(expr->expressions[1],map);

      switch(expr->op[0]){
      case '+':{
         Value res = MakeValue(val1.number + val2.number);
         return res;
      }break;
      case '-':{
         Value res = MakeValue(val1.number - val2.number);
         return res;
      }break;
      case '*':{
         Value res = MakeValue(val1.number * val2.number);
         return res;
      }break;
      case '/':{
         Value res = MakeValue(val1.number / val2.number);
         return res;
      }break;
      default:{
         NOT_IMPLEMENTED;
      }break;
      }
   }break;

   case Expression::IDENTIFIER:{
      return map[expr->id];
   }break;
   case Expression::LITERAL:{
      return expr->val;
   }break;
   default:{
      NOT_POSSIBLE;
   }break;
   }

   NOT_POSSIBLE;
   return MakeValue();
}

static ValueMap ParseParameters(Tokenizer* tok,ValueMap& map){
   //TODO: Add type and range to parsing
   /*
   Range currentRange;
   ParameterType type;
   */
   ValueMap parameters;

   while(1){
      Token peek = tok->PeekToken();

      if(CompareToken(peek,"parameter")){
         tok->AdvancePeek(peek);
         // Parse optional type info and range
         continue;
      } else if(CompareToken(peek,")")){
         break;
      } else if(CompareToken(peek,";")){ // To parse inside module parameters, technically wrong but harmless
         tok->AdvancePeek(peek);
         break;
      } else if(CompareToken(peek,",")){
         tok->AdvancePeek(peek);
         continue;
      } else { // Must be a parameter assignemnt
         Token paramName = tok->NextToken();

         tok->AssertNextToken("=");

         Expression* expr = ParseAtom(tok);

         Value val = Eval(expr,map);

         map[paramName] = val;
         parameters[paramName] = val;
      }
   }

   return parameters;
}

static Expression* ParseExpression(Tokenizer* tok){
   Expression* res = ParseOperationType(tok,{{"+","-"},{"*","/"}},ParseFactor,tempArena);

   return res;
}

static RangeAndExpr ParseRange(Tokenizer* tok,ValueMap& map){
   Token peek = tok->PeekToken();

   if(!CompareString(peek,"[")){
      RangeAndExpr range = {};

      return range;
   }

   tok->AssertNextToken("[");

   RangeAndExpr res = {};
   res.top = ParseExpression(tok);
   res.range.high = Eval(res.top,map).number;

   tok->AssertNextToken(":");

   res.bottom = ParseExpression(tok);
   res.range.low = Eval(res.bottom,map).number;
   tok->AssertNextToken("]");

   return res;
}

static Module ParseModule(Tokenizer* tok){
   Module module = {};
   ValueMap values;

   tok->AssertNextToken("module");

   module.name = tok->NextToken();

   Token peek = tok->PeekToken();
   if(CompareToken(peek,"#(")){
      tok->AdvancePeek(peek);
      module.parameters = ParseParameters(tok,values);
      tok->AssertNextToken(")");
   }

   tok->AssertNextToken("(");

   // Parse ports
   while(!tok->Done()){
      peek = tok->PeekToken();

      PortDeclaration port;

      if(CompareToken(peek,"(*")){
         tok->AdvancePeek(peek);
         while(1){
            Token attributeName = tok->NextToken();

            if(!CompareString(attributeName,"versat_latency")){
               printf("ERROR: Do not know attribute named: %.*s\n",UNPACK_SS(attributeName));
               exit(-1);
            }

            peek = tok->PeekToken();
            if(CompareString(peek,"=")){
               tok->AdvancePeek(peek);
               Expression* expr = ParseExpression(tok);
               Value value = Eval(expr,values);

               peek = tok->PeekToken();

               port.attributes[attributeName] = value;
            }

            if(CompareString(peek,",")){
               tok->AdvancePeek(peek);
               continue;
            }
            if(CompareString(peek,"*)")){
               tok->AdvancePeek(peek);
               break;
            }
         }
      }

      Token portType = tok->NextToken();
      if(CompareString(portType,"input")){
         port.type = PortDeclaration::INPUT;
      } else if(CompareString(portType,"output")){
         port.type = PortDeclaration::OUTPUT;
      } else if(CompareString(portType,"inout")){
         port.type = PortDeclaration::INOUT;
      } else {
         UNHANDLED_ERROR;
      }

      peek = tok->PeekToken();

      // TODO: Add a new function to parser to "ignore" the following list of tokens (loop every time until it doesn't find one from the list), and replace this function here with reg and all the different types it can be
      if(CompareString("reg",peek)){
         tok->AdvancePeek(peek);
      }

      RangeAndExpr res = ParseRange(tok,values);
      port.range = res.range;
      port.top = res.top;
      port.bottom = res.bottom;
      port.name = tok->NextToken();

      module.ports.push_back(port);

      peek = tok->PeekToken();
      if(CompareToken(peek,")")){
         tok->AdvancePeek(peek);
         break;
      }

      tok->AssertNextToken(",");
   }

   // Any inside module parameters
   #if 0
   while(1){
      Token peek = tok->PeekToken();

      if(CompareToken(peek,"parameter")){
         ParseParameters(tok,values);
      } else if(CompareToken(peek,"endmodule")){
         tok->AdvancePeek(peek);
         break;
      } else {
         tok->AdvancePeek(peek);
      }
   }
   #endif

   Token skip = tok->PeekFindIncluding("endmodule");
   tok->AdvancePeek(skip);

   return module;
}

std::vector<Module> ParseVerilogFile(String fileContent, std::vector<const char*>* includeFilepaths, Arena* arena){
   tempArena = arena;

   Tokenizer tokenizer = Tokenizer(fileContent,":,()[]{}\"+-/*=",{"#(","+:","-:","(*","*)","module","endmodule"});
   Tokenizer* tok = &tokenizer;

   std::vector<Module> modules;
   Byte* mark = MarkArena(tempArena);

   bool isSource = false;
   while(!tok->Done()){
      Token peek = tok->PeekToken();

      if(CompareToken(peek,"(*")){
         tok->AdvancePeek(peek);

         Token attribute = tok->NextToken();
         if(CompareToken(attribute,"source")){
            isSource = true;
         } else {
            NOT_IMPLEMENTED; // Unknown attribute, error for now
         }

         tok->AssertNextToken("*)");

         continue;
      }

      if(CompareToken(peek,"module")){
         Module module = ParseModule(tok);

         module.isSource = isSource;
         modules.push_back(module);

         isSource = false;
         break; // For now, only parse the first module found
      }

      tok->AdvancePeek(peek);
   }
   PopMark(tempArena,mark);

   return modules;
}

#if 0
#define STANDALONE
#endif

#ifdef STANDALONE
// Empty impl otherwise debug won't work on template engine
void StartDebugTerminal(){
}
void DebugTerminal(Value val){
}

#include "templateData.inc"

int main(int argc,const char* argv[]){
   if(argc < 4){
      printf("Error, need at least 4 arguments: <program> <namespace> <outputFile> <inputFile1> ...");

      return 0;
   }

   RegisterTypes();

   std::vector<const char*> includePaths;
   std::vector<const char*> filePaths;
   // Parse include paths
   for(int i = 3; i < argc; i++){
      if(CompareString("-I",argv[i])){
         includePaths.push_back(argv[i+1]);
         i += 1;
      } else {
         filePaths.push_back(argv[i]);
      }
   }

   Arena tempArenaInst = InitArena(Megabyte(256));
   tempArena = &tempArenaInst;

   Arena preprocess = InitArena(Megabyte(256));
   Arena permanent = InitArena(Megabyte(256));

   std::vector<ModuleInfo> allModules;
   for(const char* str : filePaths){
      Byte* mark = MarkArena(tempArena);
      printf("%s\n",str);

      String content = PushFile(tempArena,str);

      if(content.size <= 0){
         printf("Failed to open file: %s\n",str);
         return 0;
      }

      String processed = PreprocessVerilogFile(&preprocess,content,&includePaths,tempArena);
      std::vector<Module> modules = ParseVerilogFile(processed,&includePaths,tempArena);

      for(Module& module : modules){
         BLOCK_REGION(tempArena);

         ModuleInfo info = {};
         info.inputDelays = PushArray<int>(&permanent,100).data;
         info.outputLatencies = PushArray<int>(&permanent,100).data;
         info.configs = PushArray<Wire>(&permanent,100).data;
         info.states = PushArray<Wire>(&permanent,100).data;

         info.name = module.name;
         info.isSource = module.isSource;

         Hashmap<ExternalMemoryID,ExternalMemoryInfo>* external = PushHashmap<ExternalMemoryID,ExternalMemoryInfo>(tempArena,100);

         for(PortDeclaration decl : module.ports){
            Tokenizer port(decl.name,"",{"in","out","delay","done","rst","clk","run","running","databus"});

            if(CheckFormat("ext_dp_%s_%d_port_%d",decl.name)){
               Array<Value> values = ExtractValues("ext_dp_%s_%d_port_%d",decl.name,tempArena);

               ExternalMemoryID id = {};
               id.interface = values[1].number;
               id.type = ExternalMemoryType::DP;

               String wire = values[0].str;
               int port = values[2].number;

               Assert(port < 2);

               ExternalMemoryInfo* info = external->GetOrInsert(id,{});
               if(CompareString(wire,"addr")){
                  info->ports[port].addrSize = decl.range.high - decl.range.low + 1;
               } else if(CompareString(wire,"out")){
                  info->ports[port].dataOutSize = decl.range.high - decl.range.low + 1;
               } else if(CompareString(wire,"in")){
                  info->ports[port].dataInSize = decl.range.high - decl.range.low + 1;
               } else if(CompareString(wire,"write")){
                  info->ports[port].write = true;
               } else if(CompareString(wire,"enable")){
                  info->ports[port].enable = true;
               }
            } else if(CheckFormat("ext_2p_%s",decl.name)){
               ExternalMemoryID id = {};
               id.type = ExternalMemoryType::TWO_P;

               String wire = {};
               if(CheckFormat("ext_2p_%s_%s_%d",decl.name)){
                  Array<Value> values = ExtractValues("ext_2p_%s_%s_%d",decl.name,tempArena);

                  wire = values[0].str;
                  id.interface = values[2].number;
               } else if(CheckFormat("ext_2p_%s_%d",decl.name)){
                  Array<Value> values = ExtractValues("ext_2p_%s_%d",decl.name,tempArena);

                  wire = values[0].str;
                  id.interface = values[1].number;
               } else {
                  UNHANDLED_ERROR;
               }

               ExternalMemoryInfo* info = external->GetOrInsert(id,{});

               if(CompareString(wire,"addr")){
                  info->ports[0].addrSize = decl.range.high - decl.range.low + 1;
               } else if(CompareString(wire,"data")){
                  info->ports[0].dataInSize = decl.range.high - decl.range.low + 1;
                  info->ports[0].dataOutSize = decl.range.high - decl.range.low + 1;
               } else if(CompareString(wire,"write")){
                  info->ports[0].write = true;
               } else if(CompareString(wire,"read")){
                  info->ports[0].write = true;
               } else {
                  UNHANDLED_ERROR;
               }
            } else if(CheckFormat("in%d",decl.name)){
               port.AssertNextToken("in");
               int input = ParseInt(port.NextToken());
               int delay = decl.attributes[STRING("versat_latency")].number;

               info.nInputs = std::max(info.nInputs,input + 1);
               info.inputDelays[input] = delay;
            } else if(CheckFormat("out%d",decl.name)){
               port.AssertNextToken("out");
               int output = ParseInt(port.NextToken());
               int latency = decl.attributes[STRING("versat_latency")].number;

               info.nOutputs = std::max(info.nOutputs,output + 1);
               info.outputLatencies[output] = latency;
            } else if(CheckFormat("delay%d",decl.name)){
               port.AssertNextToken("delay");
               int delay = ParseInt(port.NextToken());

               info.nDelays = std::max(info.nDelays,delay + 1);
            } else if(  CheckFormat("databus_ready",decl.name)
                     || CheckFormat("databus_valid",decl.name)
                     || CheckFormat("databus_addr",decl.name)
                     || CheckFormat("databus_rdata",decl.name)
                     || CheckFormat("databus_wdata",decl.name)
                     || CheckFormat("databus_wstrb",decl.name)
                     || CheckFormat("databus_len",decl.name)
                     || CheckFormat("databus_last",decl.name)){
               info.doesIO = true;
            } else if(  CheckFormat("ready",decl.name)
                     || CheckFormat("valid",decl.name)
                     || CheckFormat("addr",decl.name)
                     || CheckFormat("rdata",decl.name)
                     || CheckFormat("wdata",decl.name)
                     || CheckFormat("wstrb",decl.name)){
               info.memoryMapped = true;

               if(CheckFormat("addr",decl.name)){
                  info.memoryMappedBits = (decl.range.high - decl.range.low + 1);
               }
            } else if(CheckFormat("clk",decl.name)){
               info.hasClk = true;
            } else if(CheckFormat("rst",decl.name)){
               info.hasReset = true;
            } else if(CheckFormat("run",decl.name)){
               info.hasRun = true;
            } else if(CheckFormat("running",decl.name)){
               info.hasRunning = true;
            } else if(CheckFormat("done",decl.name)){
               info.hasDone = true;
            } else if(decl.type == PortDeclaration::INPUT){ // Config
               info.configs[info.nConfigs].bitsize = decl.range.high - decl.range.low + 1;
               info.configs[info.nConfigs++].name = decl.name;
            } else if(decl.type == PortDeclaration::OUTPUT){ // State
               info.states[info.nStates].bitsize = decl.range.high - decl.range.low + 1;
               info.states[info.nStates++].name = decl.name;
            } else {
               NOT_IMPLEMENTED;
            }
         }

         Array<ExternalMemoryInterface> interfaces = PushArray<ExternalMemoryInterface>(&permanent,external->nodesUsed);
         int index = 0;
         for(Pair<ExternalMemoryID,ExternalMemoryInfo> pair : external){
            ExternalMemoryInterface& inter = interfaces[index++];

            inter.interface = pair.first.interface;
            inter.type = pair.first.type;
            inter.bitsize = pair.second.ports[0].addrSize;
            inter.datasize = pair.second.ports[0].dataInSize;
         }
         info.externalInterfaces = interfaces;

         allModules.push_back(info);
      }
   }

   TemplateSetArray("modules","ModuleInfo",allModules.data(),allModules.size());

   FILE* output = OpenFileAndCreateDirectories(argv[2],"w");
   TemplateSetString("namespace",argv[1]);
   fflush(stdout);
   CompiledTemplate* comp = CompileTemplate(unit_verilog_data,tempArena);
   ProcessTemplate(output,comp,tempArena);

   return 0;
}
#endif











