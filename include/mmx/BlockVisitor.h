/*
 * BlockVisitor.h
 *
 *  Created on: Dec 29, 2021
 *      Author: mad
 */

#ifndef INCLUDE_MMX_BLOCKVISITOR_H_
#define INCLUDE_MMX_BLOCKVISITOR_H_

#include <vnx/Visitor.h>

#include <mmx/hash_t.hpp>


namespace mmx {

class BlockVisitor : public vnx::Visitor {
public:
	BlockVisitor(	vnx::InputStream& stream,
					std::unordered_map<hash_t, std::pair<int64_t, uint32_t>>& tx_index)
		:	stream(stream), tx_index(tx_index)
	{
	}

	void visit_null() override {}
	void visit(const bool& value) override {}
	void visit(const uint8_t& value) override {}
	void visit(const uint16_t& value) override {}
	void visit(const uint32_t& value) override {}
	void visit(const uint64_t& value) override {}
	void visit(const int8_t& value) override {}
	void visit(const int16_t& value) override {}
	void visit(const int32_t& value) override {}
	void visit(const int64_t& value) override {}
	void visit(const vnx::float32_t& value) override {}
	void visit(const vnx::float64_t& value) override {}
	void visit(const std::string& value) override {}

	void list_begin(size_t size) override {}
	void list_element(size_t index) override {}
	void list_end(size_t size) override {}

	void type_begin(size_t num_fields, const std::string& type_name = std::string()) override {}

private:
	vnx::InputStream& stream;
	std::unordered_map<hash_t, std::pair<int64_t, uint32_t>>& tx_index;

};


} // mmx

#endif /* INCLUDE_MMX_BLOCKVISITOR_H_ */
