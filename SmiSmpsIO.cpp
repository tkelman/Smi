// Copyright (C) 2003, International Business Machines
// Corporation and others.  All Rights Reserved.

#include "SmiSmpsIO.hpp"
#include <cassert>
#include <cmath>
#include <cfloat>
#include <string>
#include <cstdio>
#include <iostream>

#include "CoinMpsIO.hpp"
#include "CoinMessage.hpp"
#include "CoinError.hpp"


#if 1
const static char *section[] = {
  "", "NAME", "ENDATA", " ", "PERIODS", "SCENARIOS", " "
};

const static char *smpsType[] = {
	"SC","  ","DISCRETE","ADD","REPLACE",""
};
#endif


//#############################################################################

SmiCoreData *
SmiSmpsIO::readTimeFile(SmiScnModel *smi, const char *c, const char *ext)
{

	FILE *fp=NULL;
	gzFile gzfp=NULL;
	int returnCode = this->dealWithFileName(c,ext,fp,gzfp);
	if (returnCode<0){
		return NULL;
	}
	else if (returnCode>0) {
//		delete cardReader_;
		smpsCardReader_ = new SmiSmpsCardReader ( fp , gzfp, this);
	}
	
	smpsCardReader_->readToNextSection();
	if ( smpsCardReader_->whichSection (  ) == COIN_NAME_SECTION ) {
		iftime = true;
		// check problem name and issue warning if needed
		if (strcmp(problemName_,smpsCardReader_->columnName()))
		{
			printf("Warning: Time file name %s does not match problem file name %s\n",
				smpsCardReader_->columnName(),problemName_);
		}
	} else if ( smpsCardReader_->whichSection (  ) == COIN_UNKNOWN_SECTION ) {
		handler_->message(COIN_MPS_BADFILE1,messages_)<<smpsCardReader_->card()
			<<1
			<<fileName_
			<<CoinMessageEol;
#ifdef COIN_USE_ZLIB
		if (!smpsCardReader_->filePointer()) 
			handler_->message(COIN_MPS_BADFILE2,messages_)<<CoinMessageEol;
		
#endif
		return NULL;
	} else if ( smpsCardReader_->whichSection (  ) != COIN_EOF_SECTION ) {
		// save name of section
		free(problemName_);
		problemName_=strdup(smpsCardReader_->card());
	} else {
		handler_->message(COIN_MPS_EOF,messages_)<<fileName_
			<<CoinMessageEol;
		return NULL;
	}

	if (iftime)
	{
		int rowEnd, rowStart = 0;
		int colEnd, colStart = 0;
		if (	smpsCardReader_->nextSmpsField() != SMI_TIME_SECTION ) // PERIODS card
			return NULL;
	
		if (	smpsCardReader_->nextSmpsField() == SMI_TIME_SECTION ) // first period
		{
		
			cstag_ = new int[this->getNumCols()];
			rstag_ = new int[this->getNumRows()];
			periodMap_.insert(make_pair(smpsCardReader_->periodName(),nstag_));
		}
		else
			return NULL;

		// don't handle the unordered case yet
		if (smpsCardReader_->whichSmpsType() != SMI_TIME_ORDERED_CORE_TYPE)
			return NULL;

		while( smpsCardReader_->nextSmpsField (  ) == SMI_TIME_SECTION ) 
		{
			if (smpsCardReader_->whichSmpsType() == SMI_TIME_ORDERED_CORE_TYPE)
			{
				
				colEnd = this->columnIndex(smpsCardReader_->columnName());
				rowEnd = this->rowIndex(smpsCardReader_->rowName());
				
				periodMap_.insert(make_pair(smpsCardReader_->periodName(),nstag_+1));
				
				int i;
				for(i=colStart;i<colEnd;++i)
					cstag_[i]=nstag_;
				for(i=rowStart;i<rowEnd;++i)
					rstag_[i]=nstag_;
				++nstag_;
				colStart = colEnd;
				rowStart = rowEnd;
			}
			else
				return NULL;
		}
		
		if ( smpsCardReader_->whichSmpsSection() == SMI_ENDATA_SECTION
			&& smpsCardReader_->whichSmpsType() == SMI_TIME_ORDERED_CORE_TYPE)
		{
			int i;

			for(i=colStart;i<this->getNumCols();++i)
				cstag_[i]=nstag_;
			for(i=rowStart;i<this->getNumRows();++i)
				rstag_[i]=nstag_;
		}
		else
			return NULL;

	}


	SmiCoreData *smiCoreData = new SmiCoreData(this,nstag_+1,cstag_,rstag_);
	return smiCoreData;

}

//#############################################################################

int
SmiSmpsIO::readStochFile(SmiScnModel *smi,SmiCoreData *core, const char *c, const char *ext)
{
	
	FILE *fp=NULL;
	gzFile gzfp=NULL;
	int returnCode = this->dealWithFileName(c,ext,fp,gzfp);
	if (returnCode<0){
		return -1;
	}
	else if (returnCode>0) {
		delete smpsCardReader_;
		smpsCardReader_ = new SmiSmpsCardReader ( fp , gzfp, this);
	}
	
	smpsCardReader_->readToNextSection();
	if ( smpsCardReader_->whichSection (  ) == COIN_NAME_SECTION ) {
		ifstoch = true;
		// check problem name and issue warning if needed
		if (strcmp(problemName_,smpsCardReader_->columnName()))
		{
			printf("Warning: Time file name %s does not match problem file name %s\n",
				smpsCardReader_->columnName(),problemName_);
		}
	} else if ( smpsCardReader_->whichSection (  ) == COIN_UNKNOWN_SECTION ) {
		handler_->message(COIN_MPS_BADFILE1,messages_)<<smpsCardReader_->card()
			<<1
			<<fileName_
			<<CoinMessageEol;
#ifdef COIN_USE_ZLIB
		if (!smpsCardReader_->filePointer()) 
			handler_->message(COIN_MPS_BADFILE2,messages_)<<CoinMessageEol;
		
#endif
		return -2;
	} else if ( smpsCardReader_->whichSection (  ) != COIN_EOF_SECTION ) {
		// save name of section
		free(problemName_);
		problemName_=strdup(smpsCardReader_->card());
	} else {
		handler_->message(COIN_MPS_EOF,messages_)<<fileName_
			<<CoinMessageEol;
		return -3;
	}

	if (ifstoch)
	{
		if (smpsCardReader_->nextSmpsField() == SMI_SCENARIOS_SECTION) // SCENARIOS card
		{
			double prob=0.0;
			int scen=0,anc=0;
			int branch=0;
			CoinPackedVector drlo,drup,dclo,dcup,dobj;
			CoinPackedMatrix *matrix = new CoinPackedMatrix(false,0.25,0.25);
			assert(!matrix->isColOrdered());

			int nrow = core->getNumRows();
			int ncol = core->getNumCols();
			matrix->setDimensions(nrow,ncol);

			while( smpsCardReader_->nextSmpsField (  ) == SMI_SCENARIOS_SECTION ) 
			{
				if (smpsCardReader_->whichSmpsType() == SMI_SC_CARD)
				{
					if (scen)
					{
						
						smi->generateScenario(core,matrix,&dclo,&dcup,&dobj,&drlo,&drup,branch,anc,prob,smpsCardReader_->getCoreCombineRule() );
						matrix->clear();
						dclo.clear();
						dcup.clear();
						dobj.clear();
						drlo.clear();
						drup.clear();
						
						matrix->setDimensions(nrow,ncol);

					}

					scenarioMap_.insert(make_pair(smpsCardReader_->scenarioNew(),scen++));
					prob = smpsCardReader_->value();
					if (!strncmp(smpsCardReader_->scenarioAnc(),"ROOT",4))
						anc = 0;
					else
						anc = scenarioMap_[smpsCardReader_->scenarioAnc()];
					branch = periodMap_[smpsCardReader_->periodName()];
				}
				else if(smpsCardReader_->whichSmpsType() == SMI_COLUMN_CARD)
				{
					int j=this->columnIndex(smpsCardReader_->columnName());
					int i=this->rowIndex(smpsCardReader_->rowName());
					double value = smpsCardReader_->value();

					
					if (j<0) // check RHS
					{
						assert(!strcmp(smpsCardReader_->columnName(),this->getRhsName()));
						assert(!(i<0));

						char c=this->getRowSense()[i];

						switch(c)
						{
						case 'E':
							drlo.insert(i,value);
							drup.insert(i,value);
							break;
						case 'L':
							drlo.insert(i,value);
							break;
						case 'G':
							drup.insert(i,value);
							break;
						default:
							assert(!"bad row sense: shouldn't get here");
							break;
						}
					}
					else if(i<0) // check OBJ
					{
						assert(!strcmp(smpsCardReader_->rowName(),this->getObjectiveName()));
						dobj.insert(j,value);
					}
					else	// add element
					{
						matrix->modifyCoefficient(i,j,value);
					}
				}
				
				else // unknown type
					return -1;

			} // while

			if (smpsCardReader_->whichSmpsSection() == SMI_ENDATA_SECTION)
				{
					if (scen)
					{
						
						smi->generateScenario(core,matrix,&dclo,&dcup,&dobj,&drlo,&drup,branch,anc,prob,smpsCardReader_->getCoreCombineRule() );
						
					}
				}

		} else {
			return -1;
		}

		

	}



	return 0;
}

//#############################################################################
//  nextSmpsField

// function defs from CoinMpsIO
char * nextBlankOr( char *image );
void strcpyAndCompress ( char *to, const char *from );
double osi_strtod(char * ptr, char ** output) ;


SmiSectionType
SmiSmpsCardReader::nextSmpsField (  )
{
  
  // find next non blank character
  char *next = position_;

  static const char *blanks = (const char *) " \t";
  char valstr[MAX_FIELD_LENGTH],*after;
  
  while ( next != eol_ ) {
    if ( *next == ' ' || *next == '\t' ) {
      next++;
    } else {
      break;
    }
  }

  bool gotCard;
  
  if ( next == eol_ ) {
	  gotCard = false;
  } else {
	  gotCard = true;
  }
  while ( !gotCard ) 
  {
	  // need new image
	  
	  if ( cleanCard() ) {
		  return SMI_EOF_SECTION;
	  }
	  if ( card_[0] == ' ' ) {
		  // not a section or comment
		  position_ = card_;
		  eol_ = card_ + strlen ( card_ );


		  if ( smiSection_ == SMI_TIME_SECTION ) 
		  {
			  smiSmpsType_ = SMI_TIME_ORDERED_CORE_TYPE;
			  if (!(next = strtok(position_,blanks)))
			  {
				  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
				  return smiSection_;
			  }
			  strcpy(columnName_,next);
			  
			  if (!(next = strtok(NULL,blanks)))
			  {
				  strcpy(periodName_,columnName_);
				  smiSmpsType_ = SMI_TIME_UNORDERED_CORE_TYPE;
				  return smiSection_;
			  }
			  strcpy(rowName_,next);
			  if (!(next = strtok(NULL,blanks)))
			  {
				  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
				  return smiSection_;
			  }
			  strcpy(periodName_,next);
			  position_ = eol_;
			  
		  }
		  if ( smiSection_ == SMI_SCENARIOS_SECTION )
		  {
			

			  
			  if (!(next = strtok(position_,blanks)))
			  {
				  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
				  return smiSection_;
			  }
			
			  int i;
			  if (strlen(next)==2)
			  {
				  for (i = SMI_SC_CARD; i < SMI_UNKNOWN_MPS_TYPE; ++i) {
					  if ( !strncmp ( next, smpsType[i], 2 ) ) 
					  {
						  break;
					  }
				  }
			  }
			  else
				  i = SMI_COLUMN_CARD;
			
			  switch(smiSmpsType_ = (SmiSmpsType) i)
			  {
			  case SMI_SC_CARD:
				  if (!(next = strtok(NULL,blanks)))
				  {
					  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
					  break;
				  }
				  strcpy(columnName_,next);
				  
				  if (!(next = strtok(NULL,blanks)))
				  {
					  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
					  break;
				  }
				  strcpy(rowName_,next);

				  if (!(next = strtok(NULL,blanks)))
				  {
					  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
					  break;
				  }
				  strcpy(valstr,next);

				  value_ = osi_strtod(valstr,&after);
				  // see if error
				  assert(after>valstr);

				  if (!(next = strtok(NULL,blanks)))
				  {
					  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
					  break;
				  }
				  strcpy(periodName_,next);

				  // no futher strings allowed.
				  position_=eol_;
				  break;
			  case SMI_COLUMN_CARD:

				  //already got the name
				  strcpy(columnName_,next);
				  
				  if (!(next = strtok(NULL,blanks)))
				  {
					  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
					  break;
				  }
				  strcpy(rowName_,next);

				  if (!(next = strtok(NULL,blanks)))
				  {
					  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
					  break;
				  }
				  strcpy(valstr,next);

				  value_ = osi_strtod(valstr,&after);
				  // see if error
				  assert(after>valstr);
				  
				  if (!(next = strtok(NULL,blanks)))
					  position_ = eol_;
				  else
					  strcpy(next,periodName_); // store it here until next time
				  break;
			  default:
				  assert(smiSmpsType_ == SMI_UNKNOWN_MPS_TYPE);
				  break;
			  }
		  }
		  
		  return smiSection_;

	  } else if ( card_[0] != '*' ) {
		  // not a comment, might be a section
		  int i;
		  
		  handler_->message(COIN_MPS_LINE,messages_)<<cardNumber_
			  <<card_<<CoinMessageEol;

		  // find the section, if there is one
		  for ( i = SMI_NAME_SECTION; i < SMI_UNKNOWN_SECTION; i++ ) {
			  if ( !strncmp ( card_, section[i], strlen ( section[i] ) ) ) {
				  break;
			  }
		  }
		  position_ = card_;
		  eol_ = card_;
		  smiSection_ = ( SmiSectionType ) i;

		  // if its a scenario card, need to process some more info
		  if (smiSection_ == SMI_SCENARIOS_SECTION)
		  {
			  next = strtok(position_,blanks);
			  
			  if (!(next = strtok(NULL,blanks)))
			  {
				  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
				  break;
			  }
			  // find the section, if there is one
			  // next card should be DISCRETE
			  if (!(next = strtok(NULL,blanks)))
			  {
				  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
				  break;
			  }
			  // find the section, if there is one
			  
			  for ( i = SMI_SMPS_COMBINE_ADD; i < SMI_SMPS_COMBINE_UNKNOWN; i++ ) {
				  if ( !strncmp ( next, smpsType[i], strlen ( section[i] ) ) ) {
					  break;
				  }
			  }
			  
			  switch(i)
			  {
			  case SMI_SMPS_COMBINE_ADD:
				  this->setCoreCombineRule(SmiCoreCombineAdd::Instance());
				  break;
			  case SMI_SMPS_COMBINE_REPLACE:
				  this->setCoreCombineRule(SmiCoreCombineReplace::Instance());
				  break;
			  default:
				  this->setCoreCombineRule(SmiCoreCombineReplace::Instance());
				  // MESSAGE
				  printf(" Smps: setting default core combine rule to Replace\n");
			  }
			  
		  }
		  return smiSection_;
	  } else {
		  // comment
	  }
  }
  
  {
	  // if we get here, there are additional row fields on the line

				  strcpy(rowName_,periodName_); // stored the last field in periodName_
				  
				  if (!(next = strtok(NULL,blanks)))
				  {
					  smiSmpsType_ = SMI_UNKNOWN_MPS_TYPE;
					  return smiSection_;
				  }
				  strcpy(valstr,next);
				  
				  value_ = osi_strtod(valstr,&after);
				  // see if error
				  assert(after>valstr);

				  if (!(next = strtok(NULL,blanks)))
					  position_ = eol_;
				  else
					  strcpy(next,periodName_);
				  
				  
				  return smiSection_;
  }
}

