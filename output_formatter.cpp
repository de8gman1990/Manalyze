/*
This file is part of Spike Guard.

Spike Guard is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Spike Guard is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Spike Guard.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "output_formatter.h"

namespace io {

// ----------------------------------------------------------------------------

void RawFormatter::format(std::ostream& sink, bool end_stream)
{
	static bool print_header = true;
	if (_header != "" && print_header) 
	{
		sink << _header << std::endl << std::endl;
		print_header = false;
	}

	pNodes n = _root->get_children();
	
	for (nodes::const_iterator it = n->begin() ; it != n->end() ; ++it) // File level
	{
		_dump_node(sink, *it, determine_max_width(*it));
	}
	_root->clear(); // Free all the nodes that were already printed. Keeps the RAM in check for recursive analyses.
}

// ----------------------------------------------------------------------------

void RawFormatter::_dump_node(std::ostream& sink, pNode node, int max_width, int level)
{
	if (*node->get_name() == "Plugins") // Handle plugin output separately.
	{
		_dump_plugin_node(sink, node);
		return;
	}

	if (level == 0) // File level
	{
		sink << "-------------------------------------------------------------------------------" << std::endl;
		sink << *node->get_name() << std::endl;
		sink << "-------------------------------------------------------------------------------" << std::endl << std::endl;
	}
	else if (level == 1) // Category level
	{
		if (node->get_type() != OutputTreeNode::LIST)
		{
			PRINT_WARNING << "[RawFormatter] Root element of an analysis is not a list!" << std::endl;
			return;
		}
		sink << *node->get_name() << ":" << std::endl << std::string(node->get_name()->length() + 1, '-') << std::endl;
	}
	else if (level == 2) 
	{
		sink << *node->get_name();
		if (node->get_type() == OutputTreeNode::LIST) {
			sink << ":" << std::endl;
		}
	}
	else 
	{
		sink << std::string((level - 2) * 4, ' ') << *node->get_name();
		if (node->get_type() == OutputTreeNode::LIST) {
			sink << ":" << std::endl;
		}
	}

	switch (node->get_type())
	{
		case OutputTreeNode::LIST:
			{ // New scope to be able to declare the "children" variable.
				// Determine children's max width
				pNodes children = node->get_children();
				for (nodes::const_iterator it = children->begin(); it != children->end(); ++it) 
				{
					// Dump all children with an increased indentation level.
					if ((*it)->get_type() == OutputTreeNode::LIST) {
						_dump_node(sink, *it, determine_max_width(*it), level + 1);
					}
					else {
						_dump_node(sink, *it, max_width, level + 1);
					}
				}
				sink << std::endl;
			}
			break;

		case OutputTreeNode::STRINGS:
			_dump_strings_node(sink, node, max_width, level);
			break;

		default:
			// TODO: Respect the HIDE_NAME modifier
			if (max_width > 0) {
				sink << ": " << std::string(max_width - node->get_name()->length(), ' ') << *node->to_string() << std::endl;
			}
			else {
				sink << ": " << *node->to_string() << std::endl;
			}
			break;
	}
}

// ----------------------------------------------------------------------------

void RawFormatter::_dump_plugin_node(std::ostream& sink, pNode node)
{
	if (node->get_type() != OutputTreeNode::LIST)
	{
		PRINT_WARNING << "[RawFormatter] Plugins node is not a LIST!" << std::endl;
		return;
	}

	pNodes plugin_nodes = node->get_children();
	for (nodes::const_iterator it = plugin_nodes->begin() ; it != plugin_nodes->end() ; ++it)
	{
		pNode level = (*it)->find_node("level");
		pNode summary = (*it)->find_node("summary");
		pNode info = (*it)->find_node("plugin_output");
		if (!info)
		{
			PRINT_WARNING << "[RawFormatter] No output for plugin " << *(*it)->get_name() << "!" << std::endl;
			continue;
		}

		if (level)
		{
			switch (level->get_level())
			{
			case plugin::NO_OPINION:
				break;

			case plugin::MALICIOUS:
				utils::print_colored_text("MALICIOUS", utils::RED, sink, "[ ", " ] ");
				break;

			case plugin::SUSPICIOUS:
				utils::print_colored_text("SUSPICIOUS", utils::YELLOW, sink, "[ ", " ] ");
				break;

			case plugin::SAFE:
				utils::print_colored_text("SAFE", utils::GREEN, sink, "[ ", " ] ");
				break;
			}
		}

		if (summary) {
			sink << *summary->to_string() << std::endl;
		}
		else if (level->get_level() != plugin::NO_OPINION) {
			sink << std::endl;
		}

		pNodes output = info->get_children();
		for (pNodes::element_type::const_iterator it2 = output->begin() ; it2 != output->end() ; ++it2)
		{
			switch ((*it2)->get_type())
			{
				case OutputTreeNode::STRINGS:
				case OutputTreeNode::LIST:
					_dump_node(sink, *it2, io::determine_max_width(info), 3);
					break;
				default:
					if ((*it2)->get_modifier() == OutputTreeNode::HIDE_NAME) {
						sink << "\t" << *(*it2)->to_string() << std::endl;
					}
					else {
						sink << "\t" << *(*it2)->get_name() << ": " << *(*it2)->to_string() << std::endl;
					}
					break;
			}
		}
		if (summary || output->size() > 0) {
			sink << std::endl;
		}
	}
}

void RawFormatter::_dump_strings_node(std::ostream& sink, pNode node, int max_width, int level)
{
	shared_strings strs = node->get_strings();
	if (strs->size() == 0) // Special case : empty array of strings.
	{
		if (max_width > 0) {
			sink << ": " << std::string(max_width - node->get_name()->length(), ' ') << "(EMPTY)" << std::endl;
		}
		else {
			sink << ": (EMPTY)" << std::endl;
		}
		return;
	}

	for (strings::const_iterator it = strs->begin() ; it != strs->end() ; ++it)
	{
		if (node->get_modifier() == OutputTreeNode::NEW_LINE) {
			max_width = 0; // Ignore max width if we print after a line break: alignment is based on level only.
		}

		if (max_width > 0)
		{
			if (it == strs->begin())
			{
				sink << ": " << std::string(max_width - node->get_name()->length(), ' ') << *it << std::endl;
			}
			else {
				sink << std::string(max_width + 2 + (level - 2) * 4, ' ') << *it << std::endl;
			}
		}
		else
		{
			if (it == strs->begin())
			{
				if (node->get_modifier() == OutputTreeNode::NEW_LINE)
				{
					sink << ":" << std::endl;
					// Increase level by one. Since we're printing after a line break, add a TAB  for readability.
					sink << std::string((level - 1) * 4, ' ') << *it << std::endl;
				}
				else {
					sink << ": " << *it << std::endl;
				}
			}
			else {
				if (node->get_modifier() == OutputTreeNode::NEW_LINE)
				{
					sink << std::string((level - 1) * 4, ' ') << *it << std::endl;
				}
				else {
					sink << std::string((level - 2) * 4, ' ') << *it << std::endl;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------------

void JsonFormatter::format(std::ostream& sink, bool end_stream)
{
	static bool print_header = true;
	if (print_header)
	{ 
		sink << "{" << std::endl;
		print_header = false;
	}
	
	pNodes n = _root->get_children();
	for (nodes::const_iterator it = n->begin() ; it != n->end() ; ++it) // File level
	{
		_dump_node(sink, *it);
	}

	if (end_stream) {
		sink << "}" << std::endl;
	}
	_root->clear(); // Free all the nodes that were already printed. Keeps the RAM in check for recursive analyses.
}

// ----------------------------------------------------------------------------

void JsonFormatter::_dump_node(std::ostream& sink, pNode node, int level, bool append_comma, bool print_name)
{
	if (node->get_modifier() == OutputTreeNode::HEX) { // Hexadecimal notation is not compatible with this formatter
		node->set_modifier(OutputTreeNode::NONE);	   // ({ "my_int": 0xABC } isn't valid JSON).
	}

	std::string data;

	switch (node->get_type())
	{
		case OutputTreeNode::STRINGS:
		{ // Separate scope because variable 'strs' is declared in here.
			if (print_name) {
				sink << std::string(level, '\t') << "\"" << *node->get_name() << "\": [" << std::endl;
			}
			else {
				sink << std::string(level, '\t') << "[" << std::endl;
			}
			shared_strings strs = node->get_strings();
			for (strings::const_iterator it = strs->begin() ; it != strs->end() ; ++it)
			{
				std::string str = *it;
				boost::trim(str); // Delete unnecessary whitespace
				sink << std::string(level + 1, '\t') << "\"" << str << "\"";
				if (it != strs->end() - 1) {
					sink << ",";
				}
				sink << std::endl;
			}
			sink << std::string(level, '\t') << "]";
			break;
		}
		case OutputTreeNode::LIST:
		{ // Separate scope because variable 'children' is declared in here.
			
			if (print_name) {
				sink << std::string(level, '\t') << "\"" << *node->get_name() << "\": {" << std::endl;
			}
			else {
				sink << std::string(level, '\t') << "{" << std::endl;
			}
			pNodes children = node->get_children();
			for (nodes::const_iterator it = children->begin() ; it != children->end() ; ++it)	{
				_dump_node(sink, *it, level + 1, it != children->end() - 1); // Append a comma for all elements but the last.
			}
			sink << std::string(level, '\t') << "}";
			break;
		}
		case OutputTreeNode::STRING:
			data = *node->to_string();
			boost::trim(data); // Delete unnecessary whitespace
			if (print_name) {
				sink << std::string(level, '\t') << "\"" << *node->get_name() << "\": \"" << data << "\"";
			}
			else {
				sink << std::string(level, '\t') << "\"" << data << "\"";
			}
			break;
		default:
			data = *node->to_string();
			boost::trim(data); // Delete unnecessary whitespace
			if (print_name) {
				sink << std::string(level, '\t') << "\"" << *node->get_name() << "\": " << data;
			}
			else {
				sink << std::string(level, '\t') << data;
			}
	}

	if (append_comma) {
		sink << ",";
	}
	sink << std::endl;
}

// ----------------------------------------------------------------------------

std::string uint64_to_version_number(boost::uint32_t msbytes, boost::uint32_t lsbytes)
{
	std::stringstream ss;
	ss << ((msbytes >> 16) & 0xFFFF) << "." << (msbytes & 0xFFFF) << ".";
	ss << ((lsbytes >> 16) & 0xFFFF) << "." << (lsbytes & 0xFFFF);
	return ss.str();
}

// ----------------------------------------------------------------------------

std::string timestamp_to_string(boost::uint64_t epoch_timestamp)
{
	static std::locale loc(std::cout.getloc(), new boost::posix_time::time_facet("%Y-%b-%d %H:%M:%S%F %z"));
	std::stringstream ss;
	ss.imbue(loc);
	ss << boost::posix_time::from_time_t(epoch_timestamp);
	return ss.str();
}

} // !namespace io