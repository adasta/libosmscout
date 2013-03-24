/*
  This source is part of the libosmscout library
  Copyright (C) 2011  Tim Teulings

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

/*
  This source is part of the libosmscout library
  Copyright (C) 2011  Tim Teulings

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <osmscout/oss/Parser.h>

#include <sstream>

#include <osmscout/system/Assert.h>

#include <osmscout/util/String.h>

#include <osmscout/oss/Scanner.h>

namespace osmscout {
namespace oss {


void Parser::SynErr(int n)
{
  if (errDist >= minErrDist) {
    errors->SynErr(la->line, la->col, n);
  }
  errDist = 0;
}

void Parser::SemErr(const char* msg)
{
  if (errDist >= minErrDist) {
    errors->Error(t->line, t->col, msg);
  }
  errDist = 0;
}

void Parser::SemWarning(const char* msg)
{
  errors->Warning(t->line, t->col, msg);
}

void Parser::Get()
{
  for (;;) {
    t = la;
    la = scanner->Scan();
    if (la->kind <= maxT) {
      ++errDist;
      break;
    }

    if (dummyToken != t) {
      dummyToken->kind = t->kind;
      dummyToken->pos = t->pos;
      dummyToken->col = t->col;
      dummyToken->line = t->line;
      dummyToken->next = NULL;
      coco_string_delete(dummyToken->val);
      dummyToken->val = coco_string_create(t->val);
      t = dummyToken;
    }
    la = t;
  }
}

void Parser::Expect(int n)
{
  if (la->kind==n) {
    Get();
  }
  else {
    SynErr(n);
  }
}

void Parser::ExpectWeak(int n, int follow)
{
  if (la->kind == n) {
    Get();
  }
  else {
    SynErr(n);
    while (!StartOf(follow)) {
      Get();
    }
  }
}

bool Parser::WeakSeparator(int n, int syFol, int repFol)
{
  if (la->kind == n) {
    Get();
    return true;
  }
  else if (StartOf(repFol)) {
    return false;
  }
  else {
    SynErr(n);
    while (!(StartOf(syFol) || StartOf(repFol) || StartOf(0))) {
      Get();
    }
    return StartOf(syFol);
  }
}

void Parser::OSS() {
		while (!(la->kind == _EOF || la->kind == 7 /* "OSS" */)) {SynErr(95); Get();}
		Expect(7 /* "OSS" */);
		if (la->kind == 9 /* "ORDER" */) {
			WAYORDER();
		}
		while (la->kind == 21 /* "CONST" */) {
			CONST();
		}
		while (la->kind == 13 /* "SYMBO" */) {
			SYMBOL();
		}
		while (StartOf(1)) {
			StyleFilter filter; 
			STYLE(filter);
		}
		Expect(8 /* "END" */);
		
}

void Parser::WAYORDER() {
		Expect(9 /* "ORDER" */);
		Expect(10 /* "WAYS" */);
		size_t priority=1;
		while (la->kind == 11 /* "GROUP" */) {
			WAYGROUP(priority);
			priority++;
		}
}

void Parser::CONST() {
		Expect(21 /* "CONST" */);
		while (la->kind == 22 /* "COLOR" */) {
			CONSTDEF();
			Expect(16 /* ";" */);
		}
}

void Parser::SYMBOL() {
		Expect(13 /* "SYMBO" */);
		std::string name;
		
		IDENT(name);
		SymbolRef symbol=new Symbol(name);
		
		while (la->kind == 14 /* "POLYGON" */ || la->kind == 18 /* "RECTANGLE" */ || la->kind == 20 /* "CIRCLE" */) {
			if (la->kind == 14 /* "POLYGON" */) {
				POLYGON(*symbol);
			} else if (la->kind == 18 /* "RECTANGLE" */) {
				RECTANGLE(*symbol);
			} else {
				CIRCLE(*symbol);
			}
		}
		if (!config.RegisterSymbol(symbol)) {
		 std::string e="Map symbol '"+symbol->GetName()+"' is already defined";
		 SemErr(e.c_str());
		}
		
}

void Parser::STYLE(StyleFilter filter) {
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		if (la->kind == 15 /* "{" */) {
			Get();
			while (StartOf(1)) {
				STYLE(filter);
			}
			Expect(17 /* "}" */);
		} else if (la->kind == 43 /* "NODE" */ || la->kind == 47 /* "WAY" */ || la->kind == 49 /* "AREA" */) {
			STYLEDEF(filter);
		} else SynErr(96);
}

void Parser::WAYGROUP(size_t priority) {
		Expect(11 /* "GROUP" */);
		if (la->kind == _ident) {
			std::string wayTypeName;
			TypeId      wayType;
			
			IDENT(wayTypeName);
			wayType=config.GetTypeConfig()->GetWayTypeId(wayTypeName);
			
			if (wayType==typeIgnore) {
			  std::string e="Unknown way type '"+wayTypeName+"'";
			  SemErr(e.c_str());
			}
			else {
			  config.SetWayPrio(wayType,priority);
			}
			
		}
		while (la->kind == 12 /* "," */) {
			std::string wayTypeName;
			TypeId      wayType;
			
			Get();
			IDENT(wayTypeName);
			wayType=config.GetTypeConfig()->GetWayTypeId(wayTypeName);
			
			if (wayType==typeIgnore) {
			  std::string e="Unknown way type '"+wayTypeName+"'";
			 SemErr(e.c_str());
			}
			else {
			  config.SetWayPrio(wayType,priority);
			}
			
		}
}

void Parser::IDENT(std::string& value) {
		Expect(_ident);
		value=t->val;
		
}

void Parser::POLYGON(Symbol& symbol) {
		Expect(14 /* "POLYGON" */);
		StyleFilter         filter;
		FillPartialStyle    style;
		PolygonPrimitiveRef polygon(new PolygonPrimitive(style.style));
		Coord               coord;
		
		COORD(coord);
		polygon->AddCoord(coord); 
		COORD(coord);
		polygon->AddCoord(coord); 
		while (la->kind == _number || la->kind == _double) {
			COORD(coord);
			polygon->AddCoord(coord); 
		}
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(97); Get();}
		Expect(15 /* "{" */);
		while (StartOf(2)) {
			FILLSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 3);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(98); Get();}
		Expect(17 /* "}" */);
		symbol.AddPrimitive(polygon); 
}

void Parser::RECTANGLE(Symbol& symbol) {
		Expect(18 /* "RECTANGLE" */);
		StyleFilter       filter;
		FillPartialStyle  style;
		Coord             topLeft;
		double            width;
		double            height;
		
		COORD(topLeft);
		DOUBLE(width);
		Expect(19 /* "x" */);
		DOUBLE(height);
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(99); Get();}
		Expect(15 /* "{" */);
		while (StartOf(2)) {
			FILLSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 3);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(100); Get();}
		Expect(17 /* "}" */);
		symbol.AddPrimitive(new RectanglePrimitive(topLeft,
		                                          width,height,
		                                          style.style));
		
}

void Parser::CIRCLE(Symbol& symbol) {
		Expect(20 /* "CIRCLE" */);
		Coord             center;
		double            radius;
		StyleFilter       filter;
		FillPartialStyle  style;
		
		COORD(center);
		DOUBLE(radius);
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(101); Get();}
		Expect(15 /* "{" */);
		while (StartOf(2)) {
			FILLSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 3);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(102); Get();}
		Expect(17 /* "}" */);
		symbol.AddPrimitive(new CirclePrimitive(center,
		                                       radius,
		                                       style.style));
		
}

void Parser::COORD(Coord& coord) {
		double x;
		double y;
		
		DOUBLE(x);
		Expect(12 /* "," */);
		DOUBLE(y);
		coord=Coord(x,y); 
}

void Parser::FILLSTYLEATTR(FillPartialStyle& style) {
		switch (la->kind) {
		case 50 /* "color" */: {
			Color fillColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(fillColor);
			style.style->SetFillColor(fillColor);
			style.attributes.insert(FillStyle::attrFillColor);
			
			break;
		}
		case 59 /* "pattern" */: {
			std::string patternName; 
			Get();
			Expect(51 /* ":" */);
			STRING(patternName);
			style.style->SetPattern(patternName);
			style.attributes.insert(FillStyle::attrPattern);
			
			break;
		}
		case 60 /* "patternMinMag" */: {
			Magnification minMag; 
			Get();
			Expect(51 /* ":" */);
			MAG(minMag);
			style.style->SetPatternMinMag(minMag);
			style.attributes.insert(FillStyle::attrPatternMinMag);
			
			break;
		}
		case 61 /* "borderColor" */: {
			Color borderColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(borderColor);
			style.style->SetBorderColor(borderColor);
			style.attributes.insert(FillStyle::attrBorderColor);
			
			break;
		}
		case 62 /* "borderWidth" */: {
			double width; 
			Get();
			Expect(51 /* ":" */);
			DISPLAYSIZE(width);
			style.style->SetBorderWidth(width);
			style.attributes.insert(FillStyle::attrBorderWidth);
			
			break;
		}
		case 63 /* "borderDash" */: {
			std::vector<double> dashes;
			double              dash;
			
			Get();
			Expect(51 /* ":" */);
			DOUBLE(dash);
			dashes.push_back(dash); 
			while (la->kind == 12 /* "," */) {
				Get();
				DOUBLE(dash);
				dashes.push_back(dash); 
			}
			style.style->SetBorderDashes(dashes);
			style.attributes.insert(FillStyle::attrBorderDashes);
			
			break;
		}
		default: SynErr(103); break;
		}
}

void Parser::DOUBLE(double& value) {
		if (la->kind == _number) {
			Get();
			if (!StringToDouble(t->val,value)) {
			 std::string e="Cannot parse double '"+std::string(t->val)+"'";
			
			 SemErr(e.c_str());
			}
			
		} else if (la->kind == _double) {
			Get();
			if (!StringToDouble(t->val,value)) {
			 std::string e="Cannot parse double '"+std::string(t->val)+"'";
			
			 SemErr(e.c_str());
			}
			
		} else SynErr(104);
}

void Parser::CONSTDEF() {
		COLORCONSTDEF();
}

void Parser::COLORCONSTDEF() {
		std::string      name;
		StyleVariableRef variable;
		Color            color;
		
		Expect(22 /* "COLOR" */);
		IDENT(name);
		variable=config.GetVariableByName(name);
		
		if (variable.Valid()) {
		 std::string e="Constant already defined";
		
		 SemErr(e.c_str());
		}
		
		Expect(23 /* "=" */);
		COLOR(color);
		if (!errors->hasErrors) {
		 config.AddVariable(name,new StyleVariableColor(color));
		}
		
}

void Parser::COLOR(Color& color) {
		if (la->kind == _color) {
			Get();
			std::string c(t->val);
			
			if (c.length()!=7 &&
			   c.length()!=9) {
			std::string e="Illegal color value";
			
			 SemErr(e.c_str());
			}
			
			if (!errors->hasErrors) {
			 ToRGBA(c,color);
			}
			
		} else if (la->kind == _variable) {
			Get();
			StyleVariableRef variable=config.GetVariableByName(t->val+1);
			
			if (!variable.Valid()) {
			 std::string e="Variable not defined";
			
			 SemErr(e.c_str());
			}
			else if (dynamic_cast<StyleVariableColor*>(variable.Get())==NULL) {
			 std::string e="Variable is not of type 'COLOR'";
			
			 SemErr(e.c_str());
			}
			
			if (!errors->hasErrors) {
			 StyleVariableColor* colorVariable=dynamic_cast<StyleVariableColor*>(variable.Get());
			
			 color=colorVariable->GetColor();
			}
			
		} else SynErr(105);
}

void Parser::STYLEFILTER(StyleFilter& filter) {
		Expect(24 /* "[" */);
		if (la->kind == 25 /* "TYPE" */) {
			TypeSet     types;
			std::string name;
			
			Get();
			IDENT(name);
			TypeId type=config.GetTypeConfig()->GetTypeId(name);
			
			if (type==typeIgnore) {
			 std::string e="Unknown type '"+name+"'";
			
			 SemErr(e.c_str());
			}
			else if (filter.HasTypes() &&
			        !filter.HasType(type)) {
			 std::string e="Type '"+name+"' is not included by parent filter";
			
			 SemErr(e.c_str());
			}
			else {
			 types.SetType(type);
			}
			
			while (la->kind == 12 /* "," */) {
				std::string name; 
				Get();
				IDENT(name);
				TypeId      type=config.GetTypeConfig()->GetTypeId(name);
				
				if (type==typeIgnore) {
				 std::string e="Unknown type '"+name+"'";
				
				 SemErr(e.c_str());
				}
				else if (filter.HasTypes() &&
				        !filter.HasType(type)) {
				 std::string e="Type '"+name+"' is not included by parent filter";
				
				 SemErr(e.c_str());
				}
				else {
				 types.SetType(type);
				}
				
			}
			filter.SetTypes(types); 
		}
		if (la->kind == 26 /* "MAG" */) {
			Get();
			if (StartOf(4)) {
				Magnification magnification; 
				MAG(magnification);
				size_t level=magnification.GetLevel();
				
				if (level<filter.GetMinLevel()) {
				std::string e="The magnification interval start is not within the parent magnification range";
				
				SemErr(e.c_str());
				}
				else {
				 filter.SetMinLevel(level);
				}
				
			}
			Expect(27 /* "-" */);
			if (StartOf(4)) {
				Magnification magnification; 
				MAG(magnification);
				size_t level=magnification.GetLevel();
				
				if (level>filter.GetMaxLevel()) {
				std::string e="The magnification interval end is not within the parent magnification range";
				
				SemErr(e.c_str());
				}
				else {
				 filter.SetMaxLevel(level);
				}
				
			}
		}
		if (la->kind == 28 /* "ONEWAY" */) {
			Get();
			filter.SetOneway(true);
			
		}
		if (la->kind == 29 /* "SIZE" */) {
			SizeCondition* sizeCondition; 
			Get();
			SIZECONDITION(sizeCondition);
			filter.SetSizeCondition(sizeCondition); 
		}
		Expect(30 /* "]" */);
}

void Parser::STYLEDEF(StyleFilter filter) {
		if (la->kind == 43 /* "NODE" */) {
			NODESTYLEDEF(filter);
		} else if (la->kind == 47 /* "WAY" */) {
			WAYSTYLEDEF(filter);
		} else if (la->kind == 49 /* "AREA" */) {
			AREASTYLEDEF(filter);
		} else SynErr(106);
}

void Parser::MAG(Magnification& magnification) {
		switch (la->kind) {
		case 80 /* "world" */: {
			Get();
			magnification.SetMagnification(Magnification::magWorld); 
			break;
		}
		case 81 /* "continent" */: {
			Get();
			magnification.SetMagnification(Magnification::magContinent); 
			break;
		}
		case 82 /* "state" */: {
			Get();
			magnification.SetMagnification(Magnification::magState); 
			break;
		}
		case 83 /* "stateOver" */: {
			Get();
			magnification.SetMagnification(Magnification::magStateOver); 
			break;
		}
		case 84 /* "county" */: {
			Get();
			magnification.SetMagnification(Magnification::magCounty); 
			break;
		}
		case 85 /* "region" */: {
			Get();
			magnification.SetMagnification(Magnification::magRegion); 
			break;
		}
		case 86 /* "proximity" */: {
			Get();
			magnification.SetMagnification(Magnification::magProximity); 
			break;
		}
		case 87 /* "cityOver" */: {
			Get();
			magnification.SetMagnification(Magnification::magCityOver); 
			break;
		}
		case 88 /* "city" */: {
			Get();
			magnification.SetMagnification(Magnification::magCity); 
			break;
		}
		case 89 /* "suburb" */: {
			Get();
			magnification.SetMagnification(Magnification::magSuburb); 
			break;
		}
		case 90 /* "detail" */: {
			Get();
			magnification.SetMagnification(Magnification::magDetail); 
			break;
		}
		case 91 /* "close" */: {
			Get();
			magnification.SetMagnification(Magnification::magClose); 
			break;
		}
		case 92 /* "veryClose" */: {
			Get();
			magnification.SetMagnification(Magnification::magVeryClose); 
			break;
		}
		case 93 /* "block" */: {
			Get();
			magnification.SetMagnification(Magnification::magBlock); 
			break;
		}
		default: SynErr(107); break;
		}
}

void Parser::SIZECONDITION(SizeCondition*& condition) {
		std::list<SizeCondition*> conditions;
		SizeCondition             *subCond;
		
		SIZEANDCOND(subCond);
		conditions.push_back(subCond); 
		while (la->kind == 31 /* "OR" */) {
			Get();
			SIZEANDCOND(subCond);
			conditions.push_back(subCond); 
		}
		if (conditions.size()==1) {
		 condition=conditions.front();
		}
		else {
		 SizeBoolCondition *orCondition=new SizeBoolCondition(SizeBoolCondition::boolOr);
		
		 for (std::list<SizeCondition*>::const_iterator c=conditions.begin();
		      c!=conditions.end();
		      ++c) {
		   orCondition->AddCondition(*c);
		 }
		
		 condition=orCondition;
		}
		
}

void Parser::SIZEANDCOND(SizeCondition*& condition) {
		std::list<SizeCondition*> conditions;
		SizeCondition             *subCond;
		
		SIZEBOOLCOND(subCond);
		conditions.push_back(subCond); 
		while (la->kind == 32 /* "AND" */) {
			Get();
			SIZEBOOLCOND(subCond);
			conditions.push_back(subCond); 
		}
		if (conditions.size()==1) {
		 condition=conditions.front();
		}
		else {
		 SizeBoolCondition *andCondition=new SizeBoolCondition(SizeBoolCondition::boolAnd);
		
		 for (std::list<SizeCondition*>::const_iterator c=conditions.begin();
		      c!=conditions.end();
		      ++c) {
		   andCondition->AddCondition(*c);
		 }
		
		 condition=andCondition;
		}
		
}

void Parser::SIZEBOOLCOND(SizeCondition*& condition) {
		if (la->kind == _number || la->kind == _double) {
			SIZEBINCOND(condition);
		} else if (la->kind == 33 /* "(" */) {
			Get();
			SIZECONDITION(condition);
			Expect(34 /* ")" */);
		} else if (la->kind == 35 /* "!" */) {
			Get();
			SIZEBOOLCOND(condition);
			condition=new SizeNotCondition(condition); 
		} else SynErr(108);
}

void Parser::SIZEBINCOND(SizeCondition*& condition) {
		double                        mapSize;
		BinaryOperator                op;
		double                        displaySize;
		SizeBinaryCondition::SizeUnit sizeUnit=SizeBinaryCondition::mm;
		
		DOUBLE(mapSize);
		Expect(36 /* "m" */);
		SIZEBINOPERATOR(op);
		DOUBLE(displaySize);
		if (la->kind == 37 /* "mm" */) {
			Get();
			sizeUnit=SizeBinaryCondition::mm; 
		} else if (la->kind == 38 /* "px" */) {
			Get();
			sizeUnit=SizeBinaryCondition::pixel; 
		} else SynErr(109);
		if (mapSize<=0.0) {
		std::string e="map size be greater than 0.0";
		
		SemErr(e.c_str());
		}
		else {
		 condition=new SizeBinaryCondition(op,displaySize/mapSize,sizeUnit);
		}
		
}

void Parser::SIZEBINOPERATOR(BinaryOperator& op) {
		if (la->kind == 39 /* "<" */) {
			Get();
			op=operatorLess; 
		} else if (la->kind == 40 /* "<=" */) {
			Get();
			op=operatorLessEqual; 
		} else if (la->kind == 41 /* ">=" */) {
			Get();
			op=operatorGreaterEqual; 
		} else if (la->kind == 42 /* ">" */) {
			Get();
			op=operatorGreater; 
		} else SynErr(110);
}

void Parser::NODESTYLEDEF(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 43 /* "NODE" */)) {SynErr(111); Get();}
		Expect(43 /* "NODE" */);
		Expect(44 /* "." */);
		if (la->kind == 45 /* "TEXT" */) {
			NODETEXTSTYLE(filter);
		} else if (la->kind == 46 /* "ICON" */) {
			NODEICONSTYLE(filter);
		} else SynErr(112);
}

void Parser::WAYSTYLEDEF(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 47 /* "WAY" */)) {SynErr(113); Get();}
		Expect(47 /* "WAY" */);
		if (la->kind == 15 /* "{" */ || la->kind == 24 /* "[" */) {
			WAYSTYLE(filter);
		} else if (la->kind == 44 /* "." */) {
			Get();
			if (la->kind == 45 /* "TEXT" */) {
				WAYPATHTEXTSTYLE(filter);
			} else if (la->kind == 13 /* "SYMBO" */) {
				WAYPATHSYMBOLSTYLE(filter);
			} else if (la->kind == 48 /* "SHIELD" */) {
				WAYSHIELDSTYLE(filter);
			} else SynErr(114);
		} else SynErr(115);
}

void Parser::AREASTYLEDEF(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 49 /* "AREA" */)) {SynErr(116); Get();}
		Expect(49 /* "AREA" */);
		if (la->kind == 15 /* "{" */ || la->kind == 24 /* "[" */) {
			AREASTYLE(filter);
		} else if (la->kind == 44 /* "." */) {
			Get();
			if (la->kind == 45 /* "TEXT" */) {
				AREATEXTSTYLE(filter);
			} else if (la->kind == 46 /* "ICON" */) {
				AREAICONSTYLE(filter);
			} else SynErr(117);
		} else SynErr(118);
}

void Parser::NODETEXTSTYLE(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 45 /* "TEXT" */)) {SynErr(119); Get();}
		Expect(45 /* "TEXT" */);
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		TextPartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(120); Get();}
		Expect(15 /* "{" */);
		while (StartOf(5)) {
			TEXTSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 6);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(121); Get();}
		Expect(17 /* "}" */);
		config.AddNodeTextStyle(filter,style);
		
}

void Parser::NODEICONSTYLE(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 46 /* "ICON" */)) {SynErr(122); Get();}
		Expect(46 /* "ICON" */);
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		IconPartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(123); Get();}
		Expect(15 /* "{" */);
		while (la->kind == 71 /* "symbol" */ || la->kind == 73 /* "name" */) {
			ICONSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 7);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(124); Get();}
		Expect(17 /* "}" */);
		config.AddNodeIconStyle(filter,style);
		
}

void Parser::TEXTSTYLEATTR(TextPartialStyle& style) {
		switch (la->kind) {
		case 64 /* "label" */: {
			TextStyle::Label label; 
			Get();
			Expect(51 /* ":" */);
			TEXTLABEL(label);
			style.style->SetLabel(label);
			style.attributes.insert(TextStyle::attrLabel);
			
			break;
		}
		case 65 /* "style" */: {
			TextStyle::Style labelStyle; 
			Get();
			Expect(51 /* ":" */);
			LABELSTYLE(labelStyle);
			style.style->SetStyle(labelStyle);
			style.attributes.insert(TextStyle::attrStyle);
			
			break;
		}
		case 50 /* "color" */: {
			Color textColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(textColor);
			style.style->SetTextColor(textColor);
			style.attributes.insert(TextStyle::attrTextColor);
			
			break;
		}
		case 66 /* "size" */: {
			double size; 
			Get();
			Expect(51 /* ":" */);
			DOUBLE(size);
			style.style->SetSize(size);
			style.attributes.insert(TextStyle::attrSize);
			
			break;
		}
		case 67 /* "scaleMag" */: {
			Magnification scaleMag; 
			Get();
			Expect(51 /* ":" */);
			MAG(scaleMag);
			style.style->SetScaleAndFadeMag(scaleMag);
			style.attributes.insert(TextStyle::attrScaleAndFadeMag);
			
			break;
		}
		case 68 /* "priority" */: {
			size_t priority; 
			Get();
			Expect(51 /* ":" */);
			INTEGER(priority);
			if (priority>=0 && priority<std::numeric_limits<uint8_t>::max()) {
			   style.style->SetPriority((uint8_t)priority);
			   style.attributes.insert(TextStyle::attrPriority);
			}
			else {
			 std::string e="Priority must be in the interval [0,"+
			               NumberToString(std::numeric_limits<uint8_t>::max())+"[";
			
			 SemErr(e.c_str());
			}
			
			break;
		}
		default: SynErr(125); break;
		}
}

void Parser::ICONSTYLEATTR(IconPartialStyle& style) {
		if (la->kind == 71 /* "symbol" */) {
			std::string name;
			SymbolRef   symbol;
			
			Get();
			Expect(51 /* ":" */);
			IDENT(name);
			symbol=config.GetSymbol(name);
			
			if (symbol.Invalid()) {
			 std::string e="Map symbol '"+name+"' is not defined";
			
			 SemErr(e.c_str());
			}
			else {
			 style.style->SetSymbol(symbol);
			 style.attributes.insert(IconStyle::attrSymbol);
			}
			
		} else if (la->kind == 73 /* "name" */) {
			std::string name; 
			Get();
			Expect(51 /* ":" */);
			IDENT(name);
			style.style->SetIconName(name);
			style.attributes.insert(IconStyle::attrIconName);
			
		} else SynErr(126);
}

void Parser::WAYSTYLE(StyleFilter filter) {
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		LinePartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(127); Get();}
		Expect(15 /* "{" */);
		while (StartOf(8)) {
			LINESTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 9);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(128); Get();}
		Expect(17 /* "}" */);
		config.AddWayLineStyle(filter,style);
		
}

void Parser::WAYPATHTEXTSTYLE(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 45 /* "TEXT" */)) {SynErr(129); Get();}
		Expect(45 /* "TEXT" */);
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		PathTextPartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(130); Get();}
		Expect(15 /* "{" */);
		while (la->kind == 50 /* "color" */ || la->kind == 64 /* "label" */ || la->kind == 66 /* "size" */) {
			PATHTEXTSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 10);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(131); Get();}
		Expect(17 /* "}" */);
		config.AddWayPathTextStyle(filter,style);
		
}

void Parser::WAYPATHSYMBOLSTYLE(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 13 /* "SYMBO" */)) {SynErr(132); Get();}
		Expect(13 /* "SYMBO" */);
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		PathSymbolPartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(133); Get();}
		Expect(15 /* "{" */);
		while (la->kind == 71 /* "symbol" */ || la->kind == 72 /* "symbolSpace" */) {
			PATHSYMBOLSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 11);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(134); Get();}
		Expect(17 /* "}" */);
		config.AddWayPathSymbolStyle(filter,style);
		
}

void Parser::WAYSHIELDSTYLE(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 48 /* "SHIELD" */)) {SynErr(135); Get();}
		Expect(48 /* "SHIELD" */);
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		PathShieldPartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(136); Get();}
		Expect(15 /* "{" */);
		while (StartOf(12)) {
			PATHSHIELDSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 13);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(137); Get();}
		Expect(17 /* "}" */);
		config.AddWayPathShieldStyle(filter,style);
		
}

void Parser::LINESTYLEATTR(LinePartialStyle& style) {
		switch (la->kind) {
		case 50 /* "color" */: {
			Color lineColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(lineColor);
			style.style->SetLineColor(lineColor);
			style.attributes.insert(LineStyle::attrLineColor);
			
			break;
		}
		case 52 /* "outlineColor" */: {
			Color outlineColor;
			Get();
			Expect(51 /* ":" */);
			COLOR(outlineColor);
			style.style->SetOutlineColor(outlineColor);
			style.attributes.insert(LineStyle::attrOutlineColor);
			
			break;
		}
		case 53 /* "dash" */: {
			std::vector<double> dashes;
			double              dash;
			
			Get();
			Expect(51 /* ":" */);
			DOUBLE(dash);
			dashes.push_back(dash); 
			while (la->kind == 12 /* "," */) {
				Get();
				DOUBLE(dash);
				dashes.push_back(dash); 
			}
			style.style->SetDashes(dashes);
			style.attributes.insert(LineStyle::attrDashes);
			
			break;
		}
		case 54 /* "gapColor" */: {
			Color gapColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(gapColor);
			style.style->SetGapColor(gapColor);
			style.attributes.insert(LineStyle::attrGapColor);
			
			break;
		}
		case 55 /* "displayWidth" */: {
			double displayWidth; 
			Get();
			Expect(51 /* ":" */);
			DISPLAYSIZE(displayWidth);
			style.style->SetDisplayWidth(displayWidth);
			style.attributes.insert(LineStyle::attrDisplayWidth);
			
			break;
		}
		case 56 /* "width" */: {
			double width; 
			Get();
			Expect(51 /* ":" */);
			MAPSIZE(width);
			style.style->SetWidth(width);
			style.attributes.insert(LineStyle::attrWidth);
			
			break;
		}
		case 57 /* "cap" */: {
			LineStyle::CapStyle capStyle; 
			Get();
			Expect(51 /* ":" */);
			CAPSTYLE(capStyle);
			style.style->SetCapStyle(capStyle);
			style.attributes.insert(LineStyle::attrCapStyle);
			
			break;
		}
		case 58 /* "outline" */: {
			double outline; 
			Get();
			Expect(51 /* ":" */);
			DISPLAYSIZE(outline);
			style.style->SetOutline(outline);
			style.attributes.insert(LineStyle::attrOutline);
			
			break;
		}
		default: SynErr(138); break;
		}
}

void Parser::PATHTEXTSTYLEATTR(PathTextPartialStyle& style) {
		if (la->kind == 64 /* "label" */) {
			PathTextStyle::Label label; 
			Get();
			Expect(51 /* ":" */);
			PATHTEXTLABEL(label);
			style.style->SetLabel(label);
			style.attributes.insert(PathTextStyle::attrLabel);
			
		} else if (la->kind == 50 /* "color" */) {
			Color textColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(textColor);
			style.style->SetTextColor(textColor);
			style.attributes.insert(PathTextStyle::attrTextColor);
			
		} else if (la->kind == 66 /* "size" */) {
			double size; 
			Get();
			Expect(51 /* ":" */);
			DOUBLE(size);
			style.style->SetSize(size);
			style.attributes.insert(PathTextStyle::attrSize);
			
		} else SynErr(139);
}

void Parser::PATHSYMBOLSTYLEATTR(PathSymbolPartialStyle& style) {
		if (la->kind == 71 /* "symbol" */) {
			std::string name;
			SymbolRef   symbol;
			
			Get();
			Expect(51 /* ":" */);
			IDENT(name);
			symbol=config.GetSymbol(name);
			
			if (symbol.Invalid()) {
			 std::string e="Map symbol '"+name+"' is not defined";
			
			 SemErr(e.c_str());
			}
			else {
			 style.style->SetSymbol(symbol);
			 style.attributes.insert(PathSymbolStyle::attrSymbol);
			}
			
		} else if (la->kind == 72 /* "symbolSpace" */) {
			double symbolSpace; 
			Get();
			Expect(51 /* ":" */);
			DISPLAYSIZE(symbolSpace);
			style.style->SetSymbolSpace(symbolSpace);
			style.attributes.insert(PathSymbolStyle::attrSymbolSpace);
			
		} else SynErr(140);
}

void Parser::PATHSHIELDSTYLEATTR(PathShieldPartialStyle& style) {
		switch (la->kind) {
		case 64 /* "label" */: {
			ShieldStyle::Label label; 
			Get();
			Expect(51 /* ":" */);
			SHIELDLABEL(label);
			style.style->SetLabel(label);
			style.attributes.insert(PathShieldStyle::attrLabel);
			
			break;
		}
		case 50 /* "color" */: {
			Color textColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(textColor);
			style.style->SetTextColor(textColor);
			style.attributes.insert(PathShieldStyle::attrTextColor);
			
			break;
		}
		case 69 /* "backgroundColor" */: {
			Color bgColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(bgColor);
			style.style->SetBgColor(bgColor);
			style.attributes.insert(PathShieldStyle::attrBgColor);
			
			break;
		}
		case 61 /* "borderColor" */: {
			Color borderColor; 
			Get();
			Expect(51 /* ":" */);
			COLOR(borderColor);
			style.style->SetBorderColor(borderColor);
			style.attributes.insert(PathShieldStyle::attrBorderColor);
			
			break;
		}
		case 66 /* "size" */: {
			double size; 
			Get();
			Expect(51 /* ":" */);
			DOUBLE(size);
			style.style->SetSize(size);
			style.attributes.insert(PathShieldStyle::attrSize);
			
			break;
		}
		case 68 /* "priority" */: {
			size_t priority; 
			Get();
			Expect(51 /* ":" */);
			INTEGER(priority);
			if (priority>=0 && priority<std::numeric_limits<uint8_t>::max()) {
			   style.style->SetPriority((uint8_t)priority);
			   style.attributes.insert(PathShieldStyle::attrPriority);
			}
			else {
			 std::string e="Priority must be in the interval [0,"+
			               NumberToString(std::numeric_limits<uint8_t>::max())+"[";
			
			 SemErr(e.c_str());
			}
			
			break;
		}
		case 70 /* "shieldSpace" */: {
			double shieldSpace; 
			Get();
			Expect(51 /* ":" */);
			DISPLAYSIZE(shieldSpace);
			style.style->SetShieldSpace(shieldSpace);
			style.attributes.insert(PathShieldStyle::attrShieldSpace);
			
			break;
		}
		default: SynErr(141); break;
		}
}

void Parser::AREASTYLE(StyleFilter filter) {
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		FillPartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(142); Get();}
		Expect(15 /* "{" */);
		while (StartOf(2)) {
			FILLSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 3);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(143); Get();}
		Expect(17 /* "}" */);
		config.AddAreaFillStyle(filter,style);
		
}

void Parser::AREATEXTSTYLE(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 45 /* "TEXT" */)) {SynErr(144); Get();}
		Expect(45 /* "TEXT" */);
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		TextPartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(145); Get();}
		Expect(15 /* "{" */);
		while (StartOf(5)) {
			TEXTSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 6);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(146); Get();}
		Expect(17 /* "}" */);
		config.AddAreaTextStyle(filter,style);
		
}

void Parser::AREAICONSTYLE(StyleFilter filter) {
		while (!(la->kind == _EOF || la->kind == 46 /* "ICON" */)) {SynErr(147); Get();}
		Expect(46 /* "ICON" */);
		if (la->kind == 24 /* "[" */) {
			STYLEFILTER(filter);
		}
		IconPartialStyle style;
		
		while (!(la->kind == _EOF || la->kind == 15 /* "{" */)) {SynErr(148); Get();}
		Expect(15 /* "{" */);
		while (la->kind == 71 /* "symbol" */ || la->kind == 73 /* "name" */) {
			ICONSTYLEATTR(style);
			ExpectWeak(16 /* ";" */, 7);
		}
		while (!(la->kind == _EOF || la->kind == 17 /* "}" */)) {SynErr(149); Get();}
		Expect(17 /* "}" */);
		config.AddAreaIconStyle(filter,style);
		
}

void Parser::DISPLAYSIZE(double& value) {
		DOUBLE(value);
		Expect(37 /* "mm" */);
}

void Parser::MAPSIZE(double& value) {
		DOUBLE(value);
		Expect(36 /* "m" */);
}

void Parser::CAPSTYLE(LineStyle::CapStyle& style) {
		if (la->kind == 74 /* "butt" */) {
			Get();
			style=LineStyle::capButt; 
		} else if (la->kind == 75 /* "round" */) {
			Get();
			style=LineStyle::capRound; 
		} else if (la->kind == 76 /* "square" */) {
			Get();
			style=LineStyle::capSquare; 
		} else SynErr(150);
}

void Parser::STRING(std::string& value) {
		Expect(_string);
		value=Destring(t->val);
		
}

void Parser::TEXTLABEL(TextStyle::Label& label) {
		if (la->kind == 73 /* "name" */) {
			Get();
			label=TextStyle::name; 
		} else if (la->kind == 79 /* "ref" */) {
			Get();
			label=TextStyle::ref; 
		} else SynErr(151);
}

void Parser::LABELSTYLE(TextStyle::Style& style) {
		if (la->kind == 77 /* "normal" */) {
			Get();
			style=TextStyle::normal; 
		} else if (la->kind == 78 /* "emphasize" */) {
			Get();
			style=TextStyle::emphasize; 
		} else SynErr(152);
}

void Parser::INTEGER(size_t& value) {
		Expect(_number);
		if (!StringToNumber(t->val,value)) {
		 std::string e="Cannot parse number '"+std::string(t->val)+"'";
		
		 SemErr(e.c_str());
		}
		
}

void Parser::SHIELDLABEL(ShieldStyle::Label& label) {
		if (la->kind == 73 /* "name" */) {
			Get();
			label=ShieldStyle::name; 
		} else if (la->kind == 79 /* "ref" */) {
			Get();
			label=ShieldStyle::ref; 
		} else SynErr(153);
}

void Parser::PATHTEXTLABEL(PathTextStyle::Label& label) {
		if (la->kind == 73 /* "name" */) {
			Get();
			label=PathTextStyle::name; 
		} else if (la->kind == 79 /* "ref" */) {
			Get();
			label=PathTextStyle::ref; 
		} else SynErr(154);
}



void Parser::Parse()
{
  t = NULL;
  la = dummyToken = new Token();
  la->val = coco_string_create("Dummy Token");
  Get();
	OSS();
	Expect(0);
}

Parser::Parser(Scanner *scanner,
               StyleConfig& config)
 : config(config)
{
	maxT = 94;

  dummyToken = NULL;
  t = la = NULL;
  minErrDist = 2;
  errDist = minErrDist;
  this->scanner = scanner;
  errors = new Errors();
}

bool Parser::StartOf(int s)
{
  const bool T = true;
  const bool x = false;

	static bool set[14][96] = {
		{T,x,x,x, x,x,x,T, x,x,x,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,T,T, T,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,x,x,x, x,x,x,x, T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,x,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,T,x, x,x,x,x, x,x,x,T, T,T,T,T, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{T,x,x,x, x,x,x,T, x,x,x,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,T,T, T,T,T,x, x,x,x,x, x,x,x,T, T,T,T,T, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, T,T,T,T, T,T,T,T, T,T,T,T, T,T,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,T,x, x,x,x,x, x,x,x,x, x,x,x,x, T,T,T,T, T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{T,x,x,x, x,x,x,T, x,x,x,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, T,T,T,T, T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{T,x,x,x, x,x,x,T, x,x,x,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,T,T, T,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,T,x, T,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{T,x,x,x, x,x,x,T, x,x,x,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,T,T, T,T,T,x, T,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{T,x,x,x, x,x,x,T, x,x,x,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, T,x,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{T,x,x,x, x,x,x,T, x,x,x,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,T,T, T,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, T,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,T,x, x,x,x,x, x,x,x,x, x,T,x,x, T,x,T,x, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x},
		{T,x,x,x, x,x,x,T, x,x,x,x, x,T,x,T, x,T,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,T, x,T,T,T, T,T,T,x, x,x,x,x, x,x,x,x, x,T,x,x, T,x,T,x, T,T,T,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x, x,x,x,x}
	};



  return set[s][la->kind];
}

Parser::~Parser()
{
  delete errors;
}

Errors::Errors()
 : hasErrors(false)
{
  // no code
}

void Errors::SynErr(int line, int col, int n)
{
  char* s;
  switch (n) {
			case 0: s = coco_string_create("EOF expected"); break;
			case 1: s = coco_string_create("ident expected"); break;
			case 2: s = coco_string_create("number expected"); break;
			case 3: s = coco_string_create("double expected"); break;
			case 4: s = coco_string_create("color expected"); break;
			case 5: s = coco_string_create("variable expected"); break;
			case 6: s = coco_string_create("string expected"); break;
			case 7: s = coco_string_create("\"OSS\" expected"); break;
			case 8: s = coco_string_create("\"END\" expected"); break;
			case 9: s = coco_string_create("\"ORDER\" expected"); break;
			case 10: s = coco_string_create("\"WAYS\" expected"); break;
			case 11: s = coco_string_create("\"GROUP\" expected"); break;
			case 12: s = coco_string_create("\",\" expected"); break;
			case 13: s = coco_string_create("\"SYMBOL\" expected"); break;
			case 14: s = coco_string_create("\"POLYGON\" expected"); break;
			case 15: s = coco_string_create("\"{\" expected"); break;
			case 16: s = coco_string_create("\";\" expected"); break;
			case 17: s = coco_string_create("\"}\" expected"); break;
			case 18: s = coco_string_create("\"RECTANGLE\" expected"); break;
			case 19: s = coco_string_create("\"x\" expected"); break;
			case 20: s = coco_string_create("\"CIRCLE\" expected"); break;
			case 21: s = coco_string_create("\"CONST\" expected"); break;
			case 22: s = coco_string_create("\"COLOR\" expected"); break;
			case 23: s = coco_string_create("\"=\" expected"); break;
			case 24: s = coco_string_create("\"[\" expected"); break;
			case 25: s = coco_string_create("\"TYPE\" expected"); break;
			case 26: s = coco_string_create("\"MAG\" expected"); break;
			case 27: s = coco_string_create("\"-\" expected"); break;
			case 28: s = coco_string_create("\"ONEWAY\" expected"); break;
			case 29: s = coco_string_create("\"SIZE\" expected"); break;
			case 30: s = coco_string_create("\"]\" expected"); break;
			case 31: s = coco_string_create("\"OR\" expected"); break;
			case 32: s = coco_string_create("\"AND\" expected"); break;
			case 33: s = coco_string_create("\"(\" expected"); break;
			case 34: s = coco_string_create("\")\" expected"); break;
			case 35: s = coco_string_create("\"!\" expected"); break;
			case 36: s = coco_string_create("\"m\" expected"); break;
			case 37: s = coco_string_create("\"mm\" expected"); break;
			case 38: s = coco_string_create("\"px\" expected"); break;
			case 39: s = coco_string_create("\"<\" expected"); break;
			case 40: s = coco_string_create("\"<=\" expected"); break;
			case 41: s = coco_string_create("\">=\" expected"); break;
			case 42: s = coco_string_create("\">\" expected"); break;
			case 43: s = coco_string_create("\"NODE\" expected"); break;
			case 44: s = coco_string_create("\".\" expected"); break;
			case 45: s = coco_string_create("\"TEXT\" expected"); break;
			case 46: s = coco_string_create("\"ICON\" expected"); break;
			case 47: s = coco_string_create("\"WAY\" expected"); break;
			case 48: s = coco_string_create("\"SHIELD\" expected"); break;
			case 49: s = coco_string_create("\"AREA\" expected"); break;
			case 50: s = coco_string_create("\"color\" expected"); break;
			case 51: s = coco_string_create("\":\" expected"); break;
			case 52: s = coco_string_create("\"outlineColor\" expected"); break;
			case 53: s = coco_string_create("\"dash\" expected"); break;
			case 54: s = coco_string_create("\"gapColor\" expected"); break;
			case 55: s = coco_string_create("\"displayWidth\" expected"); break;
			case 56: s = coco_string_create("\"width\" expected"); break;
			case 57: s = coco_string_create("\"cap\" expected"); break;
			case 58: s = coco_string_create("\"outline\" expected"); break;
			case 59: s = coco_string_create("\"pattern\" expected"); break;
			case 60: s = coco_string_create("\"patternMinMag\" expected"); break;
			case 61: s = coco_string_create("\"borderColor\" expected"); break;
			case 62: s = coco_string_create("\"borderWidth\" expected"); break;
			case 63: s = coco_string_create("\"borderDash\" expected"); break;
			case 64: s = coco_string_create("\"label\" expected"); break;
			case 65: s = coco_string_create("\"style\" expected"); break;
			case 66: s = coco_string_create("\"size\" expected"); break;
			case 67: s = coco_string_create("\"scaleMag\" expected"); break;
			case 68: s = coco_string_create("\"priority\" expected"); break;
			case 69: s = coco_string_create("\"backgroundColor\" expected"); break;
			case 70: s = coco_string_create("\"shieldSpace\" expected"); break;
			case 71: s = coco_string_create("\"symbol\" expected"); break;
			case 72: s = coco_string_create("\"symbolSpace\" expected"); break;
			case 73: s = coco_string_create("\"name\" expected"); break;
			case 74: s = coco_string_create("\"butt\" expected"); break;
			case 75: s = coco_string_create("\"round\" expected"); break;
			case 76: s = coco_string_create("\"square\" expected"); break;
			case 77: s = coco_string_create("\"normal\" expected"); break;
			case 78: s = coco_string_create("\"emphasize\" expected"); break;
			case 79: s = coco_string_create("\"ref\" expected"); break;
			case 80: s = coco_string_create("\"world\" expected"); break;
			case 81: s = coco_string_create("\"continent\" expected"); break;
			case 82: s = coco_string_create("\"state\" expected"); break;
			case 83: s = coco_string_create("\"stateOver\" expected"); break;
			case 84: s = coco_string_create("\"county\" expected"); break;
			case 85: s = coco_string_create("\"region\" expected"); break;
			case 86: s = coco_string_create("\"proximity\" expected"); break;
			case 87: s = coco_string_create("\"cityOver\" expected"); break;
			case 88: s = coco_string_create("\"city\" expected"); break;
			case 89: s = coco_string_create("\"suburb\" expected"); break;
			case 90: s = coco_string_create("\"detail\" expected"); break;
			case 91: s = coco_string_create("\"close\" expected"); break;
			case 92: s = coco_string_create("\"veryClose\" expected"); break;
			case 93: s = coco_string_create("\"block\" expected"); break;
			case 94: s = coco_string_create("??? expected"); break;
			case 95: s = coco_string_create("this symbol not expected in OSS"); break;
			case 96: s = coco_string_create("invalid STYLE"); break;
			case 97: s = coco_string_create("this symbol not expected in POLYGON"); break;
			case 98: s = coco_string_create("this symbol not expected in POLYGON"); break;
			case 99: s = coco_string_create("this symbol not expected in RECTANGLE"); break;
			case 100: s = coco_string_create("this symbol not expected in RECTANGLE"); break;
			case 101: s = coco_string_create("this symbol not expected in CIRCLE"); break;
			case 102: s = coco_string_create("this symbol not expected in CIRCLE"); break;
			case 103: s = coco_string_create("invalid FILLSTYLEATTR"); break;
			case 104: s = coco_string_create("invalid DOUBLE"); break;
			case 105: s = coco_string_create("invalid COLOR"); break;
			case 106: s = coco_string_create("invalid STYLEDEF"); break;
			case 107: s = coco_string_create("invalid MAG"); break;
			case 108: s = coco_string_create("invalid SIZEBOOLCOND"); break;
			case 109: s = coco_string_create("invalid SIZEBINCOND"); break;
			case 110: s = coco_string_create("invalid SIZEBINOPERATOR"); break;
			case 111: s = coco_string_create("this symbol not expected in NODESTYLEDEF"); break;
			case 112: s = coco_string_create("invalid NODESTYLEDEF"); break;
			case 113: s = coco_string_create("this symbol not expected in WAYSTYLEDEF"); break;
			case 114: s = coco_string_create("invalid WAYSTYLEDEF"); break;
			case 115: s = coco_string_create("invalid WAYSTYLEDEF"); break;
			case 116: s = coco_string_create("this symbol not expected in AREASTYLEDEF"); break;
			case 117: s = coco_string_create("invalid AREASTYLEDEF"); break;
			case 118: s = coco_string_create("invalid AREASTYLEDEF"); break;
			case 119: s = coco_string_create("this symbol not expected in NODETEXTSTYLE"); break;
			case 120: s = coco_string_create("this symbol not expected in NODETEXTSTYLE"); break;
			case 121: s = coco_string_create("this symbol not expected in NODETEXTSTYLE"); break;
			case 122: s = coco_string_create("this symbol not expected in NODEICONSTYLE"); break;
			case 123: s = coco_string_create("this symbol not expected in NODEICONSTYLE"); break;
			case 124: s = coco_string_create("this symbol not expected in NODEICONSTYLE"); break;
			case 125: s = coco_string_create("invalid TEXTSTYLEATTR"); break;
			case 126: s = coco_string_create("invalid ICONSTYLEATTR"); break;
			case 127: s = coco_string_create("this symbol not expected in WAYSTYLE"); break;
			case 128: s = coco_string_create("this symbol not expected in WAYSTYLE"); break;
			case 129: s = coco_string_create("this symbol not expected in WAYPATHTEXTSTYLE"); break;
			case 130: s = coco_string_create("this symbol not expected in WAYPATHTEXTSTYLE"); break;
			case 131: s = coco_string_create("this symbol not expected in WAYPATHTEXTSTYLE"); break;
			case 132: s = coco_string_create("this symbol not expected in WAYPATHSYMBOLSTYLE"); break;
			case 133: s = coco_string_create("this symbol not expected in WAYPATHSYMBOLSTYLE"); break;
			case 134: s = coco_string_create("this symbol not expected in WAYPATHSYMBOLSTYLE"); break;
			case 135: s = coco_string_create("this symbol not expected in WAYSHIELDSTYLE"); break;
			case 136: s = coco_string_create("this symbol not expected in WAYSHIELDSTYLE"); break;
			case 137: s = coco_string_create("this symbol not expected in WAYSHIELDSTYLE"); break;
			case 138: s = coco_string_create("invalid LINESTYLEATTR"); break;
			case 139: s = coco_string_create("invalid PATHTEXTSTYLEATTR"); break;
			case 140: s = coco_string_create("invalid PATHSYMBOLSTYLEATTR"); break;
			case 141: s = coco_string_create("invalid PATHSHIELDSTYLEATTR"); break;
			case 142: s = coco_string_create("this symbol not expected in AREASTYLE"); break;
			case 143: s = coco_string_create("this symbol not expected in AREASTYLE"); break;
			case 144: s = coco_string_create("this symbol not expected in AREATEXTSTYLE"); break;
			case 145: s = coco_string_create("this symbol not expected in AREATEXTSTYLE"); break;
			case 146: s = coco_string_create("this symbol not expected in AREATEXTSTYLE"); break;
			case 147: s = coco_string_create("this symbol not expected in AREAICONSTYLE"); break;
			case 148: s = coco_string_create("this symbol not expected in AREAICONSTYLE"); break;
			case 149: s = coco_string_create("this symbol not expected in AREAICONSTYLE"); break;
			case 150: s = coco_string_create("invalid CAPSTYLE"); break;
			case 151: s = coco_string_create("invalid TEXTLABE"); break;
			case 152: s = coco_string_create("invalid LABELSTYLE"); break;
			case 153: s = coco_string_create("invalid SHIELDLABE"); break;
			case 154: s = coco_string_create("invalid PATHTEXTLABE"); break;

    default:
    {
      std::stringstream buffer;

      buffer << "error " << n;

      s = coco_string_create(buffer.str().c_str());
    }
    break;
  }

  Err error;

  error.type=Err::Symbol;
  error.line=line;
  error.column=col;
  error.text=s;

  coco_string_delete(s);

  std::cout << error.line << "," << error.column << " " << "Symbol: " << error.text << std::endl;

  errors.push_back(error);
  hasErrors=true;
}

void Errors::Error(int line, int col, const char *s)
{
  Err error;

  error.type=Err::Error;
  error.line=line;
  error.column=col;
  error.text=s;

  std::cout << error.line << "," << error.column << " " << "Error: " << error.text << std::endl;

  errors.push_back(error);
  hasErrors=true;
}

void Errors::Warning(int line, int col, const char *s)
{
  Err error;

  error.type=Err::Warning;
  error.line=line;
  error.column=col;
  error.text=s;

  std::cout << error.line << "," << error.column << " " << "Warning: " << error.text << std::endl;

  errors.push_back(error);
}

void Errors::Warning(const char *s)
{
  Err error;

  error.type=Err::Warning;
  error.line=0;
  error.column=0;
  error.text=s;

  std::cout << error.line << "," << error.column << " " << "Warning: " << error.text << std::endl;

  errors.push_back(error);
}

void Errors::Exception(const char* s)
{
  Err error;

  error.type=Err::Exception;
  error.line=0;
  error.column=0;
  error.text=s;

  std::cout << error.line << "," << error.column << " " << "Exception: " << error.text << std::endl;

  errors.push_back(error);
  hasErrors=true;
}

} // namespace
} // namespace

