#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <conio.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#ifndef Z80
#include <dos.h>
#endif
  
#ifndef Z80                              // occhio a non superare 255 (v. GetLine e altro)
#define BUF_SIZE 160
#define MEM_SIZE 0x2000
char fStop=0;
#else
#define BUF_SIZE 80
#define MEM_SIZE 0x1800
#endif
#define VAR_SIZE 6
								 // 2 char + long
#define INT_FLAG 1
#define STR_FLAG 2

char DirectBuf[BUF_SIZE];
char *TextP;
char *PrgBase,*VarBase,*VarEnd,*StrBase,*MemTop;          // in ordine crescente
#define VERSIONE 0x101
char DirectMode=1;
char fError=0;
int Linea;

char ExecLine(void);
char ExecStmt(char);
char *CercaLine(int, char);
long EvalExpr(char, char *);
long GetValue(char);
char DoCheck(char);
char *CercaFine(char);
void SkipSpaces();
char CheckMemory(int);
char *AllocaString(int);

char *KeyWords[]={
  "SYSTEM","NEW","LIST","RUN","END","STOP","PRINT","REM","FOR","TO","STEP","NEXT","GOTO",
  "GOSUB","RETURN","IF","THEN","ELSE","ON","CALL","POKE","OUTP","INPUT","GET",
  "CLS","BEEP","SAVE","LOAD",0
  };

char *Funct[]={
  "AND","OR","NOT","SGN","ABS","INT","SQR","SIN","COS","TAN","LOG","EXP","FRE","RND","PEEK",
  "INP","LEN","STR$","VAL","CHR$","ASC","MID$","INSTR","TAB",
  "DIN",0
  };

char *Errore[]={
  "Errore di sintassi",
  "Valore non valido",
  "Riga indefinita",
  "Tipo non corrispondente",
  "RETURN senza GOSUB",
  "NEXT senza FOR",
  "Fine memoria",
  "Stringa troppo lunga",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "",
  "Fermo"
  };
  
char BitTable[8]={ 0x80,0x40,0x20,0x10,8,4,2,1 };

#ifndef Z80
void (_interrupt _far *OldCtrl_C)();
void _interrupt _far Ctrl_C();
#endif

int main() {
  char Exit=0;
  int t;
  char i,d;
  
#ifndef Z80
	OldCtrl_C=_dos_getvect(0x23);
	_dos_setvect(0x23,Ctrl_C);
#endif
	
  PrgBase=malloc(MEM_SIZE);
  MemTop=PrgBase+MEM_SIZE;
  ExecStmt('\x81');           // esegue NEW
  
ColdStart:  
  printf("\nSkyBasic v%d.%02d (C) ADPM Synthesis 1994\n",VERSIONE >> 8, VERSIONE & 0xff);
#ifndef Z80
  printf("%d bytes liberi\n",MemTop-PrgBase);
#else
  printf("%d bytes liberi",MemTop-PrgBase);
#endif  
  d=1;
WarmStart:  
  do {
	  DirectMode=1;
#ifndef Z80	  
	  fStop=0;
#endif	  
	  fError=0;
	  TextP=DirectBuf;
	  if(d)
		  puts("\nPronto.");
		GetLine();
		t=Tokenize();

/*
    i=0;
	printf("Tokenize: ");
	while(TextP[i]) {
	  printf("%02x ",(unsigned int)TextP[i++]);
	  }
//	putchar('\n');
*/
  
		if(*TextP >= '0' && *TextP <= '9') {
		  StoreLine(t);
		  d=0;
		  }
		else {
		  i=ExecLine();
//		  printf("EXEC: %x",(unsigned int)i);
		  if(i<0) {
			  if(i==-1)
					Exit=1;
			  else
					i=-i;
			  }  
		  if(i>0)
			  DoError(i);
			d=1;
		  }
		} while(!Exit);
		
#ifndef Z80
	_dos_setvect(0x23,OldCtrl_C);
#endif
  }                
  
long myAtoi() {
  long i;
  char ch;
  char m=0;
  
  i=0l;
  ch=*TextP;
  while(ch >='0' && ch <='9' || ch=='.') {
		if(ch=='.')
		  m=1;
		if(!m) {
//      i=i*10l;
		  i*=10l;
		  i+=(unsigned long)(ch-'0');
//      i=i+(unsigned long)(ch-'0');
		  }
		TextP++;
		ch=*TextP;
		}      
	
  return i;
  }

int myXtoi() {
  int i;
  char ch;
  
  i=0;
  for(;;) {
		ch=*TextP;
		if(!isxdigit(ch)) 
		  break;
		ch-='0';
		if(ch>=10) {			                  // >= è meglio di >
			if(ch>='\x30')
			  ch -= '\x20';
		  ch-=7;
		  }
		i=(i << 4)+(unsigned int)ch;
		TextP++;
		} 
	
  return i;
  }

int Tokenize() {
  register char *p;
  char *p1,*p2;
  int i,j,t;
  char *k;
  
  t=0;
  p=TextP;
  while(*p) {
//  printf("p vale %02x\n",*p);
	  if(*p>='0' && *p<='9') {
			p++;
			}   
	  else if(*p == '\"') {
			p++;
			while(*p && *p!='\"') {
			  p++;
			  t++;
			  }
			}
	  else if(isalpha(*p)) {
//          p++;
			p1=p;
//          while(isalpha(*p)) {
//            p++;
//          }   
			i=0;
			while(k=KeyWords[i]) {
			  j=strlen(k);
			  if(!strnicmp(k,p1,j)) {
//        printf("trovo %d\n",i);
					*p1++=i | 0x80;
//                  p=p+j;
					strcpy(p1,p+j);
					p=p1;
					break;
					}
			  i++;
				}
			if(!k) {
				p1=p;
					i=0;
					while(k=Funct[i]) {
					  j=strlen(k);
					  if(!strnicmp(k,p1,j)) {
							*p1++=i | 0xc0;
	//                      p=p+strlen(Funct[i]);
							strcpy(p1,p+j);
							p=p1;
							break;
							}
					  i++;
					}
				if(!k) {
				  p++;
				  }
			  }
			}
	  else if(*p=='?') {               // gestisco PRINT
			*p++=0x86;
			}
	  else if(*p=='\'') {               // gestisco REM
			*p++=0x87;
			}
	  else {
			p++;
			}
		t++;	
	  }
	return t;
  }

int GetLine() {
#ifdef Z80
  char ch;
#else
  int ch;  
#endif  
  char i;
  register char *p;

#ifdef Z80
  cursOn();
#endif  
  p=TextP;
  *p=0;
  i=0;
  ch=0;
  do {
		if(kbhit()) {
		  ch=getch();
//	    printf("letto %02x\n",(unsigned int)ch);
#ifndef Z80 
		  if(!ch) {
			  ch=0x100 | getch();
			  goto Get2;
			  }
#endif
		  if(isprint(ch)) {
				if(i < BUF_SIZE) {
				  putch(ch);
				  *p++=ch;
				  *p=0;
				  i++;
				  }
				else {
				  putch(7);
				  }
				}
			else {
Get2:
				if(ch==8) {
				  if(i>0) {
					  putch(8);
#ifndef Z80					  
					  putch(' ');
					  putch(8);
#endif					  
					  i--;
					  p--;
					  *p=0;
					  }
					else
					  putch(7);
					}
/*
#ifndef Z80					  
				else if(ch==0x14d)
#else
				else if(ch==17)
#endif					  
				  {
				  putch(*p=DirectBuf[i++]);
				  }
				  */
				}
			}
#ifndef Z80
		} while(ch != 13);
#else
_asm {
  ld e,41
  rst 8
  }
		} while(ch != 10);
#endif
#ifndef Z80
  putch(13);
#endif
  putch(10);
//  p--;
  *p=0;

#ifdef Z80
  cursOff();
#endif  

/*  i=0;
	printf("GETLINE: ");
	while(TextP[i]) {
	  printf("%02x ",TextP[i++]);
	  }
	putchar('\n');
	*/
  }

int StoreLine(int t) {
  register char **p;
  char *p1;
  int i,n;
  char m;
  
  p=TextP;
  n=myAtoi();
//  printf("n vale %x; ",n);
//  t=t-(TextP-p);
//  while(*TextP >= '0' && *TextP <= '9') {
//    TextP++;
//    t--;
//    }
  m=0;
//  p=TextP;
  while(*TextP) {                   // m=0 se si vuole cancellare la riga data
		if(*TextP != ' ') {
		  m=1;
//      p=TextP;
		  break;
		  }
		TextP++;
		}
//  TextP=p;
  t=t-(TextP-p);
  if(m) {
	  if(p=CercaLine(n,0)) {
			p1=p+2;
			i=4;
			while(*p1++)
			  i++;
//      printf("sostituisco %d a %x %x %x\n",n,p,p1,VarEnd-p1);
			memmove(p,p1,VarEnd-p1);
		  VarBase-=i;
			}
	  else {
		  if(!(p=CercaLine(n,1)))
			  p=VarBase-2;
			}
//    printf("\ap vale %x, t=%x, varbase %x\n",p,t,VarBase);
	  i=t+5;
	  memmove(((char *)p)+i,p,VarEnd-p);
	  VarBase+=i;
	  p[1]=n;
	  p1=p+2;
	  strcpy(p1,TextP);
	  *(p1+t+1)=0;
	  *p=((char *)p)+i;
//    printf("memorizzo %d, next link %x\n",t,p+i);
	  }
	else {
	  if(p=CercaLine(n,0)) {
//    printf("p vale %x, varbase %x\n",p,VarBase);
			p1=p+2;
			i=4;
			while(*p1++)
			  i++;
//      printf("cancello %d a %x %x %x\n",n,p,p1,VarEnd-p1);
		  memmove(p,p1,VarEnd-p1);
		  VarBase-=i;
//    printf("p vale %x, varbase %x\n",p,VarBase);
			}
	  }
  RelinkBasic();
  }
  
int RelinkBasic() {
  register char **p,*p1;
  
//  printf("RELINK: ");
  p=PrgBase;
  while(*p) {                    // se non è finito...
		p1=p+2;
		while(*p1++);
		*p=p1;
		p=p1;
		}
  VarBase=p+1;
  VarEnd=VarBase;
  StrBase=MemTop;
//  printf("prgbase %x, varbase %x, varend %x\n",PrgBase,VarBase,VarEnd);
  }
  
char *HandleVar(char mode, char *flags) {               // mode = 1 SET, 0 GET
  char nome[4];
#ifdef Z80  
  unsigned char i;
#else
  int i;
#endif  
  char ch,j;
  char *p;
  char *VarPtr;

  i=0;  
  *flags=0;
  *(long *)&nome=0l;
//  nome[1]=0;
//  nome[2]=0;
  SkipSpaces();
  ch=*TextP;
//  if(ch>='A' && ch<='Z' || ch>='a' && ch<='z') {
  if(isalpha(ch)) {
rifo:
	  nome[i]=*TextP++ & 0xdf;
rifo2:
	  switch(*TextP) {
			case '%':
			  *flags |= INT_FLAG;
			  nome[0] |= 0x80;
			  nome[1] |= 0x80;
			  TextP++;
			  break;
			case '$':
			  *flags |= STR_FLAG;
			  nome[1] |= 0x80;
		// manca un controllo su entrambi i flags      
			  TextP++;
			  break;
			case '(':
			  goto HndVar2;
			  break;
			default:
				ch=*TextP;
			  j=isalnum(ch);
			  if(i<1) {
					if(ch && j) {
					  i++;
					  goto rifo;
					  }
					}
			  else {
					if(ch && j) {
					  do {
//							TextP++;
							ch=*(++TextP);
							} while(ch && isalnum(ch));
					  goto rifo2;
					  }
					}
			  break;
			}
//  printf("Var: %s, modo %d, TextP %c\n",nome,(unsigned int)mode,*TextP);
	  VarPtr=0;
	  p=VarBase;
	  while(p<VarEnd) {
			if(*(int *)&nome==*(int *)p) {       // trovata
			  VarPtr=p+2;
			  break;
			  }
			p+=VAR_SIZE;
			}
	  if(!VarPtr) {
			p=VarEnd;
			*p++=nome[0];
			*p++=nome[1];
			*(long *)p=0l;
			VarPtr=VarEnd+2;
			VarEnd+=VAR_SIZE;
			}
//  printf("ritorno var: %x\n",VarPtr);
	  return VarPtr;
	  }
	else {
HndVar2:
	  fError=1;
	  return 0;
	  }
  }
  
char ExecStmt(char n) {
  char RetVal=0;
  char *p,*p1;
  char Fl,Fl1;
  char ch;
  register int i,i1;
  long l;                        // deve diventare long
  char *OldText=0,*OldVar;         // per il gosub, for
  long ToVal,StVal;              // per il for..next

	switch(n) {
//      case 0:
//        DirectMode=1;
//        break;
		case '\x80':
		  RetVal=-1;
			break;  
		case '\x81':                 // new
			*(int *)PrgBase=0;
			VarBase=PrgBase+2;
		case '\x83':                 // run/clr
doClr:
			VarEnd=VarBase;
			StrBase=MemTop;
			if(n == '\x83') {
			  if(*(int *)PrgBase) {
				  DirectMode=0;
			    TextP=PrgBase+4;
			    Linea=*(int *)(TextP-2);
			    }
			  }
			break;  
		case '\x82':                      // list
			i=GetValue(0);
			if(fError) {
			  fError=0;
			  p=PrgBase;
			  i=0x7fff;
			  }
			else {
			  p=CercaLine(i,0);
			  }
		  if(p) {
			  while(p1=(*(char **)p)) {
			    i1=*((int *)(p+2));
					if(i1>i)                  // occhio a signed...
					  break;
					printf("%u ",i1);
					p+=4;
					while(*p) {
						if(*p < 0) {
							if(*p < '\xc0')
							  printf(KeyWords[*p & 0x3f]);
							else
							  printf(Funct[*p & 0x3f]);
						  }
					  else
						  putch(*p);
					  p++;
					  }
					p++;
#ifndef Z80	  
	  			putch(13);
				  if(fStop)
					  RetVal=17;
#else
				  if(isBreak())
					  RetVal=17;
#endif
					putch(10);
				  }
				}  
			break;  
		case '\x84':                // end
		  DirectMode=1;
		  TextP=VarBase-3;
			break;  
		case '\x85':                // stop
		  RetVal=17;
			break;  
		case '\x86':                // print
		  ch=0;
myPrint:
	  	if(!fError) {
			  switch(*TextP) {
					case ';':
					  ch=1;
					case ' ':
					  TextP++;
					  goto myPrint;
					  break;
					case ',':
#ifndef Z80
						putchar('\t');
#else
						putch('\t');
#endif              
					  TextP++;
					  ch=1;
					  goto myPrint;
					  break;
					case 0:
					case ':':
					  if(!ch) {                      // indica se andare a capo...
#ifndef Z80
							putch(13);
#endif
							putch(10);
							}
					  break;
					default:
					  l=EvalExpr(15,&Fl);
					  if(fError)
						  goto myPrint;
					  if(!(Fl & STR_FLAG)) {
			  		  putch(l<0 ? '-' : ' ');
						  if(Fl & INT_FLAG) {
							  printf("%d",abs((int)l));
							  }
						  else {
							  printf("%ld",labs(l));
							  }
							}
					  else {
							p=(char *)l;
							ch=*(((char *)&l)+2);
							while(ch--)
							  putch(*p++);
							} 
					  ch=0;
					  goto myPrint;
					  break;
					}
			  }
			break;  
		case '\x87':                     // REM
myRem:
		  TextP=CercaFine(0);
		  break;
		case '\x88':                     // for
			p=HandleVar(1,&Fl);
			if(!fError) {
				OldVar=p;
				if(Fl & STR_FLAG) {
				  RetVal=4;
				  goto myFor1;
				  }
				if(DoCheck('='))
				  goto myFor1;
				l=GetValue(0);
				*(long *)OldVar=l;
				if(DoCheck('\x89'))
				  goto myFor1;
				ToVal=GetValue(0);
				if(fError)
				  goto myFor1;
				SkipSpaces();
				if(*TextP=='\x8a') {
				  TextP++;
				  StVal=GetValue(0);
					if(fError)
					  goto myFor1;
				  }
				else
				  StVal=1;
//         printf("eccomi con var=%d, To %d, Step %d, TEXTP %x\n",*(int *)OldVar,(int)ToVal,(int)StVal,*TextP);
				OldText=TextP;
				for(;;) {
					ExecLine();
					SkipSpaces();
					if(*TextP && *TextP != ':') {
						p=HandleVar(0,&Fl);
						if(p != OldVar) {
						  RetVal=6;
						  goto myFor2;
						  }
					  }
					p=TextP;
//					*(long *)OldVar=*((long *)OldVar)+StVal;
					TextP=OldText;
					*(long *)OldVar+=StVal;
					if(StVal<0) {
					  if(*((long *)OldVar) < ToVal)
							break;
					  }
				  else {
				  	if(*((long *)OldVar) > ToVal)             // patch per skynet!!!!
							break;
					  }
				  }
				TextP=p;
				}
			else {
myFor1:
			  RetVal=1;
			  }
myFor2:
		  break;
		case '\x8b':
		  RetVal=-6;
		  break;
		case '\x8d':                     // gosub
		  OldText=CercaFine(1);
//        printf("fine: %x, %x\n",OldText,*OldText);
		  RetVal=ExecStmt('\x8c');
		  ExecLine();
		  TextP=OldText;
		  break;
		case '\x8c':                     // goto
myGoto:
		  i=(int)GetValue(0);
		  if(p=CercaLine(i,0)) {
//        printf("eseguo goto %d, p=%x\n",i,p);
				TextP=p+4;
				Linea=i;
				}
		  else
				RetVal=3;
		  break;
		case '\x8e':                     // return
//        if(OldText)
//          TextP=OldText;
//        else
//          RetVal=5;
				RetVal=-5;
		  break;
		case '\x8f':                     // if
		  i=(int)GetValue(0);
		  if(i) {
				DoCheck('\x90');
				}
		  else {
				goto myRem;
				}
		  break;
//		case '\x90':                     // then
//		  break;
		case '\x92':                     // on .. goto
		  i=(int)GetValue(0);
		  DoCheck('\x8c');
		  if(i<0)
				RetVal=2;
		  else {
				if(!i)
				  goto myRem;
				while(--i) {
				  GetValue(0);
				  DoCheck(',');
				  }            
				goto myGoto;  
				}
		  break;
		case '\x93':                     // call (sys)
		  i=(int)GetValue(0);
#ifdef Z80  
_asm {
//  ld l,i
//  ld h,i+1
  jp (hl)
  }
#endif
	    break;
		case '\x94':                     // poke
		  p=(char *)GetValue(0);
		  DoCheck(',');
		  i=(int)GetValue(0);
		  *p=i;
		  break;
		case '\x95':                     // out
		  i=(int)GetValue(0);
		  DoCheck(',');
		  i1=(int)GetValue(0);
		  outp(i,i1);
		  break;
		case '\x96':                     // input
	    SkipSpaces();
		  if(*TextP=='\"') {
		    TextP++;
				while(*TextP != '\"') {
				  putch(*TextP++);
				  }
			  TextP++;
			  DoCheck(';');
				}
		  p=HandleVar(1,&Fl);
		  if(!fError) {
				putch('?');
			  putch(' ');
				scanf("%s",DirectBuf);
				if(Fl & STR_FLAG) {
				  i=strlen(DirectBuf);
//            printf("la stringa è lunga %d\n",i);
				  p1=AllocaString(i);
				  if(p1) {
					*(char **)p=p1;
					*(((int *)p)+1)=i;
					memmove(p1,DirectBuf,i);
					}
			  }
			else {
			  if(Fl & INT_FLAG) {
					*(int *)p=atoi(DirectBuf);
					}
			  else
					*(long *)p=atol(DirectBuf);
			  }
			}
		  break;
		case '\x97':                     // get
		  p=HandleVar(1,&Fl);
		  if(!fError) {
				if(Fl & STR_FLAG) {
				  if(kbhit()) {
						ch=getch();
						p1=AllocaString(1);
						if(p1) {
						  *(char **)p=p1;
						  *(((int *)p)+1)=1;
						  *p1=ch;
						  }
						}
				  else {
					  *(char **)p=StrBase;
					  *(((int *)p)+1)=0;
						}
				  }
				else
				  fError=4;
				}
		  break;
		case '\x98':
#ifndef Z80  
	  {
		union REGS ir;
		
				ir.h.ah = 6;
				ir.h.al = 0;
				ir.x.cx = 0;
				ir.h.dh = 24;
				ir.h.dl = 79;
				ir.h.bh = 7;
				int86(0x10, &ir, &ir);

				ir.h.ah = 2;
				ir.h.bh = 0;
				ir.x.dx = 0;
				int86(0x10, &ir, &ir);
				}
#else
      cls();
#endif
		  break;
		case '\x99':
#ifndef Z80  
	  	putchar(7);
#else
	  	putch(7);
#endif
		  break;
		case '\x9a':                     // save
#ifndef Z80     
		  puts("Scrittura...");
		  i=open("c:\\sky.bas",_O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY/*,_S_IREAD | _S_IWRITE*/);
		  write(i,PrgBase,VarBase-PrgBase);
		  close(i);
#endif
		  break;
		case '\x9b':                     // load
#ifndef Z80
		  puts("Lettura...");
		  i=open("c:\\sky.bas",_O_RDONLY | _O_BINARY);
		  i1=read(i,PrgBase,8000);
		  close(i);
//        VarBase=PrgBase+i1;                   // cancella variabili, compreso in Relink
#else
		  *(int *)PrgBase=PrgBase+2;              // qui fa una specie di OLD...
#endif
		  RelinkBasic();
		  break;
		case ':':
		case ' ':
		  break;
		default:
			TextP--;
//      printf("TEXTP %s\n",TextP);
			p=HandleVar(1,&Fl);
			if(!fError) {
				DoCheck('=');
			  l=EvalExpr(15,&Fl1);
			  if(fError) {
					goto myLet2;
					}
			  if(Fl & STR_FLAG) {
					if(!(Fl1 & STR_FLAG)) {
					  RetVal=4;
					  }
					else {
					  *(long *)p=l;
					  }
					}
			  else {
					if(Fl1 & STR_FLAG) {
					  RetVal=4;
					  }
					else {
						if(Fl & INT_FLAG) {
							*(int *)p=l;
							}
					  else {
							*(long *)p=l;
							}
					  }
				  }
				}
		  else {
myLet2:
				RetVal=1;
				}
		  break;
		}
  return RetVal;
	}   

char ExecLine() {
  char *p;
  char RetVal=0;
  int i;
  
rifo:
  if(!DirectMode)
		Linea=*(int *)(TextP-2);
  while(*TextP && !RetVal) {
	  RetVal=ExecStmt(*TextP++);
#ifndef Z80	  
	  if(fStop)
		  RetVal=17;
#else
_asm {
  ld e,41
  rst 8
  }
//	  doComm();
	  if(isBreak())
		  RetVal=17;
#endif
	  if(fError)
		  RetVal=fError;
	  }
	if(RetVal)
	  return RetVal;
	if(!DirectMode) {
	  TextP++;
	  if(*(int *)TextP) {
			TextP+=4;
			goto rifo;
			}
	  }
  return 0;
  }
   
char *CercaLine(int n, char m) {        //m =0 per ricerca esatta, 1 per = o superiore
  register char **p,*p1;
		  
  p=PrgBase;
  while(p1=*p) {
		if(p[1] == n)
		  return p;
		if(m) {
		  if(p[1] >= n)
			  return p;
		  }
		p=p1;
		}
  return 0;
  }
  
char *CercaFine(char m) {
  register char *p;
  
  p=TextP;
  while(*p) {
		if(m && *p==':')
		  break;
		p++;
		}
	
  return p;
  }

//#pragma code_seg text2
  
char GetAritmElem(register long *l) {
  char *p;
  char ch,Fl;
  int i,j,i1;
  long l1;
  char RetVal;
  
rifo:
  ch=*TextP;
  if(ch >= '0' && ch < ('9'+1) || ch=='.') {
		*l=myAtoi();
//    while(*TextP >= '0' && *TextP<='9')
//      TextP++;
//  printf("aritm: %ld, flags %x\n",*l,0);
		if(*TextP=='%') {
		  TextP++;
		  return INT_FLAG;
		  }
		else  
		  return 0;
		}
  else if(ch < 0) {                // prima ho i diadici...
		ch &= 0x3f;
		TextP++;
		DoCheck('(');
		switch(ch) {
		  case 2:                          // not
				l1=GetValue(0);
				*l=(!l1) ? 1 : 0;
				RetVal=INT_FLAG;
				break;
		  case 3:                          // SGN
				l1=GetValue(0);
				if(l1)
				  *l=(l1 >= 0) ? 1 : -1;
				else
				  *l=0;
				RetVal=INT_FLAG;
				break;
		  case 4:                          // ABS
				l1=GetValue(0);
				*l=labs(l1);
				RetVal=0;
				break;
		  case 5:                          // int
				l1=GetValue(0);
				*l=l1;
				RetVal=0;
				break;
		  case 6:                          // sqr
				l1=GetValue(0);
				*l=sqrt(l1);
				RetVal=0;
				break;
		  case 7:                          // sin
				l1=GetValue(0);
				*l=sin(l1);                    // sarà in gradi 360° su Z80...
				RetVal=0;
				break;
		  case 8:
				l1=GetValue(0);
				*l=cos(l1);
				RetVal=0;
				break;
		  case 9:
				l1=GetValue(0);
				*l=tan(l1);
				RetVal=0;
				break;
		  case 10:
				l1=GetValue(0);
				*l=log(l1);
				RetVal=0;
				break;
		  case 11:
				l1=GetValue(0);
				*l=exp(l1);
				RetVal=0;
				break;
		  case 12:                          // fre
	//        i=GetValue(0);
				*l=(unsigned long)(StrBase-VarEnd);
				RetVal=INT_FLAG;
				break;
		  case 13:                          // rnd
				i=GetValue(0);
				if(i<0) {
				  srand(i);
				  }
				*l=(unsigned long)rand();
				RetVal=0;
				break;
		  case 14:                          // peek
				i=GetValue(0);
				*l=(unsigned long)*((unsigned char *)i);
				RetVal=INT_FLAG;
				break;
		  case 15:                          // inp
				i=GetValue(0);
				if(i<0 || i>255) {
				  fError=2;
				  }
				else {
					*l=(unsigned long)inp(i);
					RetVal=INT_FLAG;
					}
				break;
		  case 16:                          // len
				l1=GetValue(1);
				*l=*(((int *)&l1)+1);
				RetVal=INT_FLAG;
				break;
		  case 17:                          // str
				break;
		  case 18:                          // val
				l1=GetValue(1);
				i=*(((int *)&l1)+1);
				p=((char *)l1)+i;               // truschino per le zero-term...
				ch=*p;
				*p=0;
				*l=atol((char *)l1);
				*p=ch;
				RetVal=0;
				break;
		  case 19:                          // chr
				i=GetValue(0);
				p=AllocaString(1);
				if(p) {
					*(char **)l=p;
					*(((int *)l)+1)=1;
					*p=i;
					RetVal=STR_FLAG;
					}
				break;
		  case 20:                          // asc
				l1=GetValue(1);
				*l=*(unsigned char *)l1;
				RetVal=INT_FLAG;
				break;
		  case 21:                          // mid
				l1=GetValue(1);
				if(!DoCheck(',')) {
				  i=GetValue(0);
				  i--;
				  j=*(((int *)&l1)+1);
				  if(i <= j) {
					  *(int *)l=(*(int *)&l1)+i;
					  SkipSpaces();
					  if(*TextP == ',') {
						TextP++;
						i1=GetValue(0);
						if(i1 <= (j-i)) {
						  *(((int *)l)+1)=i1;
						  }
						}
					  else {
							*(((int *)l)+1)=*(((int *)&l1)+1) -i;
							}
					  RetVal=STR_FLAG;
					  }
					else
					  fError=2;
				  }
				break;
		  case 24:                          // Digital In
				i=GetValue(0);
				i1=i & 7;
				i >>= 3;
				i=inp(i);
//        *l=i & BitTable[i1] ? 0 : 1;
				i1=BitTable[i1] ? 0 : 1;
				*l=(i & i1) ? 1 : 0;
				RetVal=INT_FLAG;
				break;
		  }
		DoCheck(')');
		return RetVal;
		}
  else if(isalpha(ch)) {
		if(toupper(ch)=='T' && toupper(*(TextP+1))=='I') {
		  TextP+=2;
		  *l=clock();
		  return 0;
		  }
		else {
			p=HandleVar(0,&Fl);
			if(!fError) {
				if(Fl & INT_FLAG) {
				  *l=(long)(*(int *)p);
				  }
				else {
				  *l=*(long *)p;
				  }
				return Fl;
				}
		  }
		}
  else {
		switch(ch) {
		  case '&':
				TextP+=2;                  // H
				*l=myXtoi();
				return INT_FLAG;
				break;
		  case '\"':
				*(char **)l=++TextP;
				i=0;
				while(*TextP != '\"' && *TextP) {
				  i++;
				  TextP++;
				  }
				*(((int *)l)+1)=i;
				if(*TextP)
				  TextP++;
				if(DirectMode) {
				  p=AllocaString(i);
				  if(p) {
						memmove(p,*(char **)l,i);
						*(char **)l=p;
						}
				  }
				return STR_FLAG;
				break;
//      case '-':
//        break;
		  case '\\':
		  case '$':
		  case '!':
		  case '#':
		  case '\'':
				fError=1;
				break;
		  case ' ':
				TextP++;
				goto rifo;
				break;
			
		  }
		}
  return -1;
  }

//#pragma code_seg text
  
char RecursEval(char Pty, register long *l1, char *f1) {
  long l2;
  char f2;
  char ch;
  char Go=0,InBrack=0,Times=0;
  char *p,*p1;
  int i,i1,j;
  
  do {
	  ch=*TextP;
//    printf("sono sul %c(%x), T %d, pty %d\n",ch,ch,Times,Pty);
	  switch(ch) {
			case '(':
			  TextP++;
			  InBrack++;
			  break;
			case ')':
			  if(InBrack) {
				  TextP++;
					InBrack--;
					}
			  else
					Go=1;
			  break;
			case '+':
			case '-':
//                printf("- ??unario: f %x,l %ld, pty %d, times %d\n",*f1,*l1,Pty,Times);
		  if(Times) {
		  if(Pty >= 5) {
				TextP++;
				  RecursEval(4,&l2,&f2);
				  if(*f1 & STR_FLAG) {
					  if(f2 & STR_FLAG) {
							i=*(((int *)l1)+1);
							i1=*(((int *)&l2)+1);
							p=AllocaString(i+i1);
							if(p) {
							  memmove(p,(char *)*(int *)l1,i);
							  memmove(p+i,(char *)*((int *)&l2),i1);
						  	*(char **)l1=p;
							  *(((int *)l1)+1)=i+i1;
						  	}
							}
					  else
						fError=4;
						}
				  else {
						if(ch=='+')
						  *l1+=l2;
						else
							*l1-=l2;
				  	}  
					}
			  else
					Go=1;
				}
		  else {
		  	if(Pty >= 3) {
					TextP++;
				  RecursEval(2,l1,f1);
				  if(ch=='-')
						*l1=-*l1;
				  if(*f1 & STR_FLAG)
						fError=4;
			  	}
			 	else
					Go=1;
			  }
		  break;
		case '*':
		case '/':
		case '^':
		case '%':
		  if(Pty >= 4) {
			  TextP++;
			  RecursEval(3,&l2,&f2);
			  if((*f1 & STR_FLAG) || (f2 & STR_FLAG)) {
					fError=4;
					}
			  else {
					switch(ch) {
					  case '*':
//					  *l1 *= l2;
					  *l1 = *l1 * l2;
					  break;
					case '/':
//					  *l1 /= l2;
					  *l1 = *l1 / l2;
					  break;
					case '^':
					  *l1=pow(*l1,l2);
					  break;
				  case '%':
//					  *l1 %= l2;
					  *l1 = *l1 % l2;
					  break;
					}
			  }
			}
		else
			Go=1;
		  break;
		case '<':
		case '=':
		case '>':
		  i=0;
		  if(Pty >= 6) {
			  TextP++;
			  if(*TextP == '=') {
					i=1;
					TextP++;
					}
			  else {
				  if(*TextP == '>') {
						i=-1;
						TextP++;
						}
				  }  
			  RecursEval(5,&l2,&f2);
			  if(*f1 & STR_FLAG) {
				  if(f2 & STR_FLAG) {
						p=*(char **)l1;
						p1=*((char **)&l2);
						i=*(((int *)l1)+1);
						i1=*(((int *)&l2)+1);
						if(i1>i)
						  i=i1;                // prendo la +lunga
					j=strncmp(p,p1,i);
						switch(ch) {
					  case '<':
							if(!i)
								*l1=j <= 0 ? 1 : 0;
						  else {
								if(i>0)
									*l1=j < 0 ? 1 : 0;
								else
									*l1=j != 0;
								}   
						  break;
					case '=':
					  *l1=j == 0;
					  break;
					case '>':
						if(i)
							*l1=j >= 0 ? 1 : 0;
					  else
							*l1=j > 0 ? 1 : 0;
					  break;
					}
				  *f1=INT_FLAG;
				}
			  else
					fError=4;
					}
			  else {
				switch(ch) {
				  case '<':
						if(!i)
							*l1=*l1 < l2;
					  else {
							if(i>0) 
							  *l1=*l1 <= l2;
							else
							  *l1=*l1 != l2;
							}
					  break;
				case '=':
				  *l1=*l1 == l2;
				  break;
				case '>':
					if(i)
						*l1=*l1 >= l2;
				  else
						*l1=*l1 > l2;
				  break;
				}
			  }
			}
		else
			Go=1;
		  break;
		case '\xc0':                        // and,or
		case '\xc1':
		  if(Pty >= 7) {
		  TextP++;
			  RecursEval(6,&l2,&f2);
			  if((*f1 & STR_FLAG) || (f2 & STR_FLAG)) {
					fError=4;
					}
			  else {
					if(ch=='\xc0')
					  *l1=(*(int *)l1) & ((int)l2);
					else
					  *l1=(*(int *)l1) | ((int)l2);
					*f1=INT_FLAG;  
				  }
				}
			else
				Go=1;
		  break;
		case ' ':
		  TextP++;
		  Times--;
		  break;
		case 0:
		case ':':
		case ';':
		case ',':
		case '\x91':                  // then,else,goto,gosub, to,step
		case '\x90':
		case '\x8c':
		case '\x8d':
		case '\x89':
		case '\x8a':
		  Go=1;
		  break;
		default:
			*f1=GetAritmElem(l1);
		  break;
		}
	  Times++;
	  } while(!Go && !fError);

  return 0;
  }
   
long EvalExpr(char Pty, char *flags) {            // deve diventare long
  long l;

  RecursEval(Pty,&l,flags);
  return l;
  }
   
long GetValue(char m) {             // m=0 number, 1 string
  char flags=-1;
  long l;

  l=EvalExpr(15,&flags);
  if(flags<0)
		fError=1;
  else { 
	  if(m) {
		  if(!(flags & STR_FLAG)) {
				fError=4;
				}
		  }
		else {
		  if(flags & STR_FLAG) {
				fError=4;
				}
		  }
	  }
  return l;
  }
   
int DoError(char n) {
  
  putch('?');
#ifdef Z80
//  beep(20);
#endif
  printf(Errore[n-1]);
  if(!DirectMode) {
	  printf(" alla linea %u\n",Linea);
	  }                    
  else {
#ifndef Z80
	  putch(10);
	  putch(13);
#endif
	  }
  }

void SkipSpaces() {
  
  while(*TextP == ' ')
		TextP++;
  }
  
char DoCheck(char ch) {
  
  SkipSpaces();
  if(ch==*TextP) {
		TextP++;
		return 0;
		}
  else {
		fError=1;
		return 1;
		}
  }
  
char CheckMemory(int n) {
  int i;
  
  i=StrBase-VarEnd;
  if(i<n) {
		fError=7;
		return 1;
		}
  else
		return 0;
  }
  
char *AllocaString(int n) {
  
  if(!CheckMemory(n)) {
		StrBase -= n;
		return StrBase;
		}
  else
		return 0;
  }
  
#ifndef Z80
#pragma check_stack(off)
void _interrupt _far Ctrl_C() {

_asm {

	mov   word ptr fStop,1
//  pop   bp
//  iret
	}
	}

#pragma check_stack(on)
#endif

