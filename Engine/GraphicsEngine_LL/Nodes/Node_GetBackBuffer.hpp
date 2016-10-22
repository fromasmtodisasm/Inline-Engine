#pragma once

#include "../GraphicsNode.hpp"

#include <cmath>


namespace inl {
namespace gxeng {
namespace nodes {


class GetBackBuffer :
	virtual public GraphicsNode,
	public exc::InputPortConfig<>,
	public exc::OutputPortConfig<RenderTargetView>
{
public:
	GetBackBuffer() {}

	virtual void Update() override {}

	virtual void Notify(exc::InputPortBase* sender) override {}

	virtual Task GetTask() override {
		return Task({ [this](const ExecutionContext& context)
		{
			auto& swapChainAccessContext = static_cast<const SwapChainAccessContext&>(context);
			this->GetOutput<0>().Set(swapChainAccessContext.GetBackBuffer()->GetView());
			return ExecutionResult{};
		} });
	}
};



} // namespace nodes
} // namespace gxeng
} // namespace inl