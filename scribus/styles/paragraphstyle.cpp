/*
 For general Scribus (>=1.3.2) copyright and licensing information please refer
 to the COPYING file provided with the program. Following this notice may exist
 a copyright and/or license notice that predates the release of Scribus 1.3.2
 for which a new license (GPL+exception) is in place.
 */
/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/
#include <QDebug>

#include "commonstrings.h"
#include "styles/style.h"
#include "paragraphstyle.h"
#include "resourcecollection.h"
#include "desaxe/saxiohelper.h"
#include "desaxe/simple_actions.h"
#include "util_math.h"

bool ParagraphStyle::TabRecord::operator==(const TabRecord& other) const
{
	return isequiv(tabPosition, other.tabPosition) && tabType == other.tabType && tabFillChar == other.tabFillChar;
}

ParagraphStyle::ParagraphStyle()
{
	setParent("");
	m_cstyleContext.setDefaultStyle( &m_cstyle );
//	qDebug() << QString("ParagraphStyle() %1 pcontext %2 ccontext %3").arg(reinterpret_cast<uint>(this)).arg(reinterpret_cast<uint>(context())).arg(reinterpret_cast<uint>(defaultStyle()->context()));
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
	m_##attr_NAME = attr_DEFAULT; \
	inh_##attr_NAME = true;
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
	
	m_isDefaultStyle = false;
}


ParagraphStyle::ParagraphStyle(const ParagraphStyle& other)
	          : BaseStyle(other), 
	            m_cstyleContextIsInh(other.m_cstyleContextIsInh),
	            m_cstyle(other.charStyle())
{
	if (m_cstyleContextIsInh)
		m_cstyle.setContext(nullptr);
	else
		m_cstyle.setContext(other.charStyle().context());
	m_cstyleContext.setDefaultStyle( &m_cstyle );
//	qDebug() << QString("ParagraphStyle(%2) %1").arg(reinterpret_cast<uint>(&other)).arg(reinterpret_cast<uint>(this));
	other.validate();

#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
	m_##attr_NAME = other.m_##attr_NAME; \
	inh_##attr_NAME = other.inh_##attr_NAME;
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
}

QString ParagraphStyle::displayName() const
{
	if (isDefaultStyle())
		return CommonStrings::trDefaultParagraphStyle;
	if (hasName() || !hasParent() || !m_context)
		return name();
//	else if ( inheritsAll() )
//		return parent()->displayName();
	return parentStyle()->displayName() + "+";
}


bool ParagraphStyle::equiv(const BaseStyle& other) const
{
	other.validate();
	const auto* oth = reinterpret_cast<const ParagraphStyle*> ( & other );
	return  oth &&
		parent() == oth->parent() && m_cstyle.equiv(oth->charStyle())
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
		&& (inh_##attr_NAME == oth->inh_##attr_NAME) \
		&& (inh_##attr_NAME || isequiv(m_##attr_NAME, oth->m_##attr_NAME))
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
		;
}	

ParagraphStyle& ParagraphStyle::operator=(const ParagraphStyle& other) 
{
	static_cast<BaseStyle&>(*this) = static_cast<const BaseStyle&>(other);

	other.validate();
	m_cstyle = other.charStyle();

	// we dont want cstyleContext to point to other's charstyle...
	m_cstyleContext.setDefaultStyle( &m_cstyle );
	
	if (m_cstyleContextIsInh)
	{
		const auto * parent = reinterpret_cast<const ParagraphStyle*> ( parentStyle() );
		m_cstyle.setContext(parent ? parent->charStyleContext() : nullptr);
	}
	else
	{
		m_cstyle.setContext(other.charStyle().context());
	}
	
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
	m_##attr_NAME = other.m_##attr_NAME; \
	inh_##attr_NAME = other.inh_##attr_NAME;
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
	return *this;
}


void ParagraphStyle::setContext(const StyleContext* context)
{
	BaseStyle::setContext(context);
//	qDebug() << QString("ParagraphStyle::setContext(%1) parent=%2").arg((unsigned long int)context).arg((unsigned long int)oth);
	repairImplicitCharStyleInheritance();
}

void ParagraphStyle::repairImplicitCharStyleInheritance()
{
	if (m_cstyleContextIsInh)
	{
		const auto * newParent = reinterpret_cast<const ParagraphStyle*> ( parentStyle() );
		m_cstyle.setContext(newParent ? newParent->charStyleContext() : nullptr);
	}
}


void ParagraphStyle::breakImplicitCharStyleInheritance(bool val)
{ 
	m_cstyleContextIsInh = !val;
	repairImplicitCharStyleInheritance();
}

void ParagraphStyle::update(const StyleContext* context)
{
	BaseStyle::update(context);
	assert ( &m_cstyleContext != m_cstyle.context());

	repairImplicitCharStyleInheritance();
	assert ( &m_cstyleContext != m_cstyle.context());
	m_cstyle.validate();
	m_cstyleContext.invalidate();

	const auto * oth = reinterpret_cast<const ParagraphStyle*> ( parentStyle() );
//	qDebug() << QString("ParagraphStyle::update(%1) parent=%2").arg((unsigned long int)context).arg((unsigned long int)oth);
	if (oth){
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
		if (inh_##attr_NAME) \
			m_##attr_NAME = oth->attr_GETTER();
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
	}
}



void ParagraphStyle::applyStyle(const ParagraphStyle& other) 
{
	if (other.hasParent() && (other.parent() != BaseStyle::INHERIT_PARENT))
	{
		setStyle(other);
		return;
	}
	BaseStyle::applyStyle(other);
	m_cstyle.applyCharStyle(other.charStyle());
	m_cstyleContext.invalidate();
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
	if (! other.inh_##attr_NAME) \
		set##attr_NAME(other.m_##attr_NAME);
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
}


void ParagraphStyle::eraseStyle(const ParagraphStyle& other) 
{
	other.validate();
	BaseStyle::eraseStyle(other);
	m_cstyle.eraseCharStyle(other.charStyle());
	m_cstyleContext.invalidate();
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
	if (!inh_##attr_NAME && m_##attr_NAME == other.m_##attr_NAME) \
		reset##attr_NAME();
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
}

void ParagraphStyle::setStyle(const ParagraphStyle & other) 
{
	other.validate();
	setParent(other.parent());
	m_contextversion = -1;
	m_cstyle.setStyle(other.charStyle());
	m_cstyleContext.invalidate();
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
	inh_##attr_NAME = other.inh_##attr_NAME; \
	m_##attr_NAME = other.m_##attr_NAME;
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
}


void ParagraphStyle::getNamedResources(ResourceCollection& lists) const
{
	for (const BaseStyle *sty = parentStyle(); sty != nullptr; sty = sty->parentStyle())
		lists.collectStyle(sty->name());
	charStyle().getNamedResources(lists);

	const QString& backgroundColorName = backgroundColor();
	if (backgroundColorName != CommonStrings::None)
		lists.collectColor(backgroundColorName);

	const QString& opticalMarginSet = opticalMarginSetId();
	if (!opticalMarginSet.isEmpty())
		lists.collectOpticalMarginSet(opticalMarginSet);

	QString parEffectStyle = peCharStyleName();
	if (parEffectStyle.length() > 0)
	{
		const CharStyle* peCharStyle = dynamic_cast<const CharStyle*>(m_cstyleContext.resolve(parEffectStyle));
		if (peCharStyle)
			peCharStyle->getNamedResources(lists);
		lists.collectCharStyle(parEffectStyle);
	}
}


void ParagraphStyle::replaceNamedResources(ResourceCollection& newNames)
{
	QMap<QString, QString>::ConstIterator it;
	
	if (hasParent() && (it = (newNames.styles().find(parent()))) != newNames.styles().end())
	{
		setParent(it.value());
		repairImplicitCharStyleInheritance();
	}

	if (!inh_BackgroundColor && (it = newNames.colors().find(backgroundColor())) != newNames.colors().end())
		setBackgroundColor(it.value());

	if (!inh_OpticalMarginSetId && (it = newNames.opticalMarginSets().find(opticalMarginSetId())) != newNames.opticalMarginSets().end())
		setOpticalMarginSetId(it.value());

	if ((it = (newNames.charStyles().find(peCharStyleName()))) != newNames.charStyles().end())
		setPeCharStyleName(it.value());
	m_cstyle.replaceNamedResources(newNames);
}


static QString toXMLString(ParagraphStyle::AlignmentType val)
{
	return QString::number(static_cast<int>(val));
}

static QString toXMLString(ParagraphStyle::DirectionType val)
{
	return QString::number(static_cast<int>(val));
}

static QString toXMLString(const QList<ParagraphStyle::TabRecord> & )
{
	return "dummy";
}

void ParagraphStyle::saxx(SaxHandler& handler, const Xml_string& elemtag) const
{
	Xml_attr att;
	BaseStyle::saxxAttributes(att);
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
	if (!inh_##attr_NAME && strcmp(# attr_NAME, "TabValues") != 0) \
		att.insert(# attr_NAME, toXMLString(m_##attr_NAME)); 
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
	if (!name().isEmpty())
		att["id"] = mkXMLName(elemtag + name());
	handler.begin(elemtag, att);
//	if (parentStyle() && hasParent())
//		parentStyle()->saxx(handler);
	if (!isInhTabValues())
	{
		for (const ParagraphStyle::TabRecord& tb : m_TabValues)
		{
			Xml_attr tab;
			tab.insert("pos", toXMLString(tb.tabPosition));
			tab.insert("fillChar", toXMLString(tb.tabFillChar.unicode()));
			tab.insert("type", toXMLString(tb.tabType));
			handler.beginEnd("tabstop", tab);
		}
	}
	if (charStyle() != CharStyle())
		charStyle().saxx(handler);
	handler.end(elemtag);
}

///   PageItem StoryText -> PageItem StoryText
class SetCharStyle_body : public desaxe::Action_body
{
	void end (const Xml_string& /*tagname*/)
	{
		ParagraphStyle* pstyle = this->dig->top<ParagraphStyle>(1);
		const CharStyle* cstyle = this->dig->top<CharStyle>(0);
		pstyle->charStyle() = *cstyle;
	}
};

class SetCharStyle : public desaxe::MakeAction<SetCharStyle_body>
{};



class SetTabStop_body : public desaxe::Action_body
{
	void begin (const Xml_string& /*tagname*/, Xml_attr attr)
	{
		ParagraphStyle* pstyle = this->dig->top<ParagraphStyle>();
		ParagraphStyle::TabRecord tb;
		tb.tabPosition = parseDouble(attr["pos"]);
		tb.tabFillChar = QChar(parseInt(attr["fillChar"]));
		tb.tabType = parseInt(attr["type"]);
		QList<ParagraphStyle::TabRecord> tabs = pstyle->tabValues();
		tabs.append(tb);
		pstyle->setTabValues(tabs);
	}
};

class SetTabStop : public desaxe::MakeAction<SetTabStop_body>
{};




template<>
ParagraphStyle::AlignmentType parse<ParagraphStyle::AlignmentType>(const Xml_string& str)
{
	return parseEnum<ParagraphStyle::AlignmentType>(str);
}

template<>
ParagraphStyle::DirectionType parse<ParagraphStyle::DirectionType>(const Xml_string& str)
{
	return parseEnum<ParagraphStyle::DirectionType>(str);
}

template<>
ParagraphStyle::LineSpacingMode parse<ParagraphStyle::LineSpacingMode>(const Xml_string& str)
{
	return parseEnum<ParagraphStyle::LineSpacingMode>(str);
}


using Tablist = QList<ParagraphStyle::TabRecord>;

template<>
Tablist parse<Tablist>(const Xml_string& str)
{
	return Tablist();
}

using namespace desaxe;

const Xml_string ParagraphStyle::saxxDefaultElem("style");

void ParagraphStyle::desaxeRules(const Xml_string& prefixPattern, Digester& ruleset, const Xml_string& elemtag)
{
	using TabRecord = ParagraphStyle::TabRecord;
		
	Xml_string stylePrefix(Digester::concat(prefixPattern, elemtag));
	ruleset.addRule(stylePrefix, Factory<ParagraphStyle>());
	ruleset.addRule(stylePrefix, IdRef<ParagraphStyle>());
	BaseStyle::desaxeRules<ParagraphStyle>(prefixPattern, ruleset, elemtag);
#define ATTRDEF(attr_TYPE, attr_GETTER, attr_NAME, attr_DEFAULT) \
	if ( strcmp(# attr_NAME, "TabValues") != 0) \
		ruleset.addRule(stylePrefix, SetAttributeWithConversion<ParagraphStyle, attr_TYPE> ( & ParagraphStyle::set##attr_NAME,  # attr_NAME, &parse<attr_TYPE> ));
#include "paragraphstyle.attrdefs.cxx"
#undef ATTRDEF
	Xml_string charstylePrefix(Digester::concat(stylePrefix, CharStyle::saxxDefaultElem));
	CharStyle::desaxeRules(stylePrefix, ruleset);
	ruleset.addRule(charstylePrefix, SetCharStyle());
	
	Xml_string tabPrefix(Digester::concat(stylePrefix, "tabstop"));
	ruleset.addRule(tabPrefix, SetTabStop());
}
