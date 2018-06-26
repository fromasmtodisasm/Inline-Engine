#include "NodeEditor.hpp"
#include <BaseLibrary/Platform/System.hpp>
#include <BaseLibrary/Range.hpp>

#include <fstream>
#include <utility>


namespace inl::tool {


static const ColorF highlightHover = { 0.3f, 0.6f, 0.9f, 0.55f };
static const ColorF highlightSelect = { 0.3f, 0.6f, 0.9f, 1.0f };
static const ColorF highlightDrag = { 0.2f, 0.9f, 0.3f, 1.0f };



NodeEditor::NodeEditor(gxeng::GraphicsEngine* graphicsEngine, IGraph* graphEditor) {
	m_graphicsEngine = graphicsEngine;
	m_graphEditor = graphEditor;

	// Create basic resources.
	m_scene.reset(m_graphicsEngine->CreateScene("MainScene"));
	m_camera.reset(m_graphicsEngine->CreateCamera2D("MainCam"));
	m_font.reset(m_graphicsEngine->CreateFont());

	m_graphicsEngine->SetShaderDirectories({ INL_NODE_SHADER_DIRECTORY, INL_MTL_SHADER_DIRECTORY, "./Shaders", "./Materials" });

	// Load pipeline.
	//std::ifstream pipelineFile(INL_NODEEDITOR_PIPELINE);
	std::ifstream pipelineFile(R"(D:\Programming\Inline-Engine\Executables\NodeEditor\pipeline.json)");
	if (!pipelineFile.is_open()) {
		throw FileNotFoundException("Failed to open pipeline JSON.");
	}
	std::string pipelineDesc((std::istreambuf_iterator<char>(pipelineFile)), std::istreambuf_iterator<char>());
	m_graphicsEngine->LoadPipeline(pipelineDesc);

	// Load a font.
	std::ifstream fontFile;
	fontFile.open(R"(C:\Windows\Fonts\calibri.ttf)", std::ios::binary);
	m_font->LoadFile(fontFile);

	// Create highlight entities.
	m_highlight1.reset(m_graphicsEngine->CreateOverlayEntity());
	m_highlight2.reset(m_graphicsEngine->CreateOverlayEntity());
	m_highlight1->SetZDepth(-1.0f);
	m_highlight2->SetZDepth(-1.0f);

	// Set camera params.
	unsigned width, height;
	m_graphicsEngine->GetScreenSize(width, height);
	m_camera->SetExtent(Vec2(width, height));
	m_camera->SetPosition(m_camera->GetExtent()/2);
	m_camera->SetVerticalFlip(true);

	// Init select panel.
	auto nameList = m_graphEditor->GetNodeList();
	m_selectPanel = std::make_unique<SelectPanel>(m_graphicsEngine, m_font.get());
	m_selectPanel->SetOptions(nameList.cbegin(), nameList.cend());
}

void NodeEditor::Update() {
	m_graphicsEngine->Update(0.001f);
}


void NodeEditor::OnResize(ResizeEvent evt) {
	m_graphicsEngine->SetScreenSize(evt.clientSize.x, evt.clientSize.y);
}

void NodeEditor::OnMouseMove(MouseMoveEvent evt) {
	Vec2 point(evt.absx, evt.absy);
	point = ScreenToWorld(point);

	const Drawable* target = Intersect(point);
	if (!m_selectedNode && !m_selectedPort) {
		if (target) {
			Hightlight(target, highlightHover);
		}
		else {
			RemoveHighlight();
		}
	}

	// Drag node.
	if (m_selectedNode && m_enableDrag) {
		Vec2 newPos = point + m_dragOffset;
		m_highlight1->SetPosition(newPos);
		m_highlight1->SetColor(highlightDrag.v);
		m_selectedNode->SetPosition(newPos);
		m_selectedNode->UpdateLinks();
	}

	// Highlight selected item.
	if (m_placing) {
		const Drawable* drawable = m_selectPanel->Intersect(point);
		SelectItem* item = const_cast<SelectItem*>(dynamic_cast<const SelectItem*>(drawable));
		if (m_highlightedItem) {
			m_highlightedItem->SetColor({1,1,1,1});
		}
		if (item) {
			m_highlightedItem = item;
			item->SetColor(highlightDrag);
		}
	}

	// Panning.
	if (m_enablePan) {
		unsigned scrWidth, scrHeight;
		m_graphicsEngine->GetScreenSize(scrWidth, scrHeight);
		Vec2 extent = m_camera->GetExtent();
		Vec2 pos = m_camera->GetPosition();

		Vec2 move = Vec2(evt.relx, evt.rely) * (extent.x/(float)scrWidth);

		m_camera->SetPosition(pos - move);
	}
}

void NodeEditor::OnMouseWheel(MouseWheelEvent evt) {
	if (m_placing) {
		int count = (int)evt.rotation;
		m_selectPanel->ScrollUp(count);
	}
	else {
		float scale = std::pow(0.95f, evt.rotation);
		Vec2 newExtent = m_camera->GetExtent()*scale;
		m_camera->SetExtent(newExtent);
	}
}

void NodeEditor::OnMouseClick(MouseButtonEvent evt) {
	Vec2 point(evt.x, evt.y);
	point = ScreenToWorld(point);

	const Drawable* target = Intersect(point);

	if (!m_placing) {
		// Select node / port.
		if (evt.button == eMouseButton::LEFT && evt.state == eKeyState::DOWN) {
			// Select/deselect node.
			if (Node* node; target && (node = const_cast<Node*>(dynamic_cast<const Node*>(target)))) {
				SelectNode(node);
				EnableDrag(node->GetPosition() - point);
			}
			else {
				SelectNode(nullptr);
			}

			// Select/deselect port.
			if (Port* port; target && (port = const_cast<Port*>(dynamic_cast<const Port*>(target)))) {
				SelectPort(port);
				EnableLink();
			}
			else {
				SelectPort(nullptr);
			}
		}

		// Commit drag / commit link.
		if (evt.button == eMouseButton::LEFT && evt.state == eKeyState::UP) {
			// Commit drag.
			if (m_enableDrag && m_selectedNode) {
				m_highlight1->SetColor(highlightSelect.v);
			}
			DisableDrag();

			// Commit link.
			Port* targetPort = const_cast<Port*>(dynamic_cast<const Port*>(target));
			Port* sourcePort = m_selectedPort;
			if (m_enableLink 
				&& sourcePort
				&& targetPort 
				&& targetPort != sourcePort
				&& targetPort->IsInput() != sourcePort->IsInput())
			{				
				if (!targetPort->IsInput()) {
					std::swap(sourcePort, targetPort);
				}

				try {
					if (sourcePort->GetParent() == targetPort->GetParent()) {
						throw LogicException("You can't link a node to itself.");
					}

					int sourceIdx = sourcePort->GetIndex();
					int targetIdx = targetPort->GetIndex();

					INode* sourceNode = sourcePort->GetParent()->GetNode();
					INode* targetNode = targetPort->GetParent()->GetNode();

					m_graphEditor->Link(sourceNode, sourceIdx, targetNode, targetIdx);

					Link link{ m_graphicsEngine, m_scene.get() };
					sourcePort->Link(targetPort, std::move(link));
				}
				catch (Exception& ex) {
					ErrorMessage(ex.Message() + ": " + ex.Subject());
				}
			}
			DisableLink();
		}

		// Delete link.
		if (evt.button == eMouseButton::LEFT && evt.state == eKeyState::DOUBLE) {
			const Link* intersect = IntersectLinks(point);
			if (intersect) {
				Port* source = const_cast<Port*>(intersect->GetSource());
				Port* target = const_cast<Port*>(intersect->GetTarget());
				target->Unlink(source);
				m_graphEditor->Unlink(target->GetParent()->GetNode(), target->GetIndex());
			}
		}
	}
	else {
		if (evt.state == eKeyState::DOUBLE) {
			const Drawable* drawable = m_selectPanel->Intersect(point);
			const SelectItem* item = dynamic_cast<const SelectItem*>(drawable);
			if (item) {
				std::string name = item->GetText();
				try {
					Node node;
					node.SetNode(m_graphEditor->AddNode(name), m_graphicsEngine, m_scene.get(), m_font.get());
					node.SetPosition(m_camera->GetPosition());
					node.SetDepth((float)m_nodes.size());
					node.SetSize(Vec2(300, 0));
					m_nodes.push_back(std::make_unique<Node>(std::move(node)));
					SelectNode(m_nodes.back().get());
				}
				catch (...) {
					assert(false);
				}
			}
		}
	}

	if (evt.button == eMouseButton::RIGHT) {
		if (evt.state == eKeyState::DOWN) {
			EnablePan();
		}
		else if (evt.state == eKeyState::UP) {
			DisablePan();
		}
	}
}

void NodeEditor::OnKey(KeyboardEvent evt) {
	if (!m_placing && evt.key == eKey::N && evt.state == eKeyState::UP) {
		EnablePlacing();
		return;
	}
	if (m_placing && (evt.key == eKey::ESCAPE || evt.key == eKey::N) && evt.state == eKeyState::UP) {
		DisablePlacing();
		return;
	}

	if (m_placing && evt.key == eKey::DOWN && evt.state == eKeyState::DOWN) {
		m_selectPanel->ScrollDown(3);
		return;
	}
	if (m_placing && evt.key == eKey::UP && evt.state == eKeyState::DOWN) {
		m_selectPanel->ScrollUp(3);
		return;
	}

	if (evt.key == eKey::DELETE && evt.state == eKeyState::UP) {
		if (m_selectedNode) {
			DeleteNode(m_selectedNode);
			SelectNode(nullptr);
		}
		return;
	}

	if (evt.key == eKey::B && evt.state == eKeyState::UP) {
		if (m_selectedNode) {
			SendToBack(m_selectedNode);
		}
	}
	else if (evt.key == eKey::F && evt.state == eKeyState::UP) {
		if (m_selectedNode) {
			SendToFront(m_selectedNode);
		}
	}
}


const Drawable* NodeEditor::Intersect(Vec2 position) const {
	float depth = std::numeric_limits<float>::lowest();
	const Drawable* result = nullptr;
	for (auto& node : m_nodes) {
		const Drawable* intersect = node->Intersect(position);
		if (intersect && intersect->GetDepth() > depth) {
			result = intersect;
			depth = intersect->GetDepth();
		}
	}
	return result;
}

const Link* NodeEditor::IntersectLinks(Vec2 position) const {
	for (auto& node : m_nodes) {
		for (auto& v : node->GetInputs()) {
			for (auto& l : v->GetLinks()) {
				auto& link = l.second;
				const Link* intersect = link->Intersect(position);
				if (intersect) {
					return intersect;
				}
			}
		}
	}
	return nullptr;
}

void NodeEditor::Hightlight(const Drawable* target, ColorF color) {
	m_highlight1->SetColor(color.v);
	m_highlight1->SetZDepth(target->GetDepth() - 0.01f);
	m_highlight1->SetPosition(target->GetPosition());
	m_highlight1->SetScale(target->GetSize() + Vec2(4, 4));

	auto& entitySet = m_scene->GetEntities<gxeng::OverlayEntity>();
	if (!entitySet.Contains(m_highlight1.get())) {
		entitySet.Add(m_highlight1.get());
	}
}

void NodeEditor::RemoveHighlight() {
	auto& entitySet = m_scene->GetEntities<gxeng::OverlayEntity>();
	if (entitySet.Contains(m_highlight1.get())) {
		entitySet.Remove(m_highlight1.get());
	}
}


void NodeEditor::SelectNode(Node* node) {
	m_selectedNode = node;
	if (node) {
		Hightlight(node, highlightSelect);
	}
	else if (!m_selectedPort) {
		RemoveHighlight();
	}
}

void NodeEditor::SelectPort(Port* port) {
	m_selectedPort = port;
	if (port) {
		Hightlight(port, highlightSelect);
	}
	else if (!m_selectedNode) {
		RemoveHighlight();
	}
}

void NodeEditor::EnableDrag(Vec2 offset) {
	m_enableDrag = true;
	m_dragOffset = offset;
}

void NodeEditor::DisableDrag() {
	m_enableDrag = false;
}

void NodeEditor::EnableLink() {
	m_enableLink = true;
}

void NodeEditor::DisableLink() {
	m_enableLink = false;
}

void NodeEditor::EnablePlacing() {
	if (!m_placing) {
		SelectNode(nullptr);
		m_selectPanel->MakeVisible(m_scene.get());
		m_selectPanel->SetPosition(m_camera->GetPosition());
		m_selectPanel->SetSize(m_camera->GetExtent() * Vec2(0.45f, 1.0f));
		m_selectPanel->SetDepth(1000.f);
		m_placing = true;
	}
}

void NodeEditor::DisablePlacing() {
	if (m_placing) {
		m_selectPanel->Hide();
		m_placing = false;
	}
	if (m_highlightedItem) {
		m_highlightedItem->SetColor({ 1,1,1,1 });
	}
}

void NodeEditor::EnablePan() {
	m_enablePan = true;
}
void NodeEditor::DisablePan() {
	m_enablePan = false;
}

void NodeEditor::SendToBack(Node* node) {
	if (node->GetDepth() < 0.001) {
		return;
	}
	int idx = FindNodeIndex(node);
	for (auto& n : m_nodes) {
		n->SetDepth(n->GetDepth() + 1);
	}
	node->SetDepth(0.0f);
	SelectNode(node);
}

void NodeEditor::SendToFront(Node* node) {
	if (node->GetDepth() > (float)m_nodes.size() - 1.001f) {
		return;
	}
	int idx = FindNodeIndex(node);
	for (auto& n : m_nodes) {
		n->SetDepth(n->GetDepth() - 1);
	}
	node->SetDepth((float)m_nodes.size() - 1.f);
	SelectNode(node);
}

void NodeEditor::DeleteNode(Node* node) {
	float depth = node->GetDepth();
	int idx = FindNodeIndex(node);
	m_nodes.erase(m_nodes.begin() + idx);
	for (auto& n : m_nodes) {
		if (n->GetDepth() > depth) {
			n->SetDepth(n->GetDepth() - 1);
		}
	}
}

void NodeEditor::ErrorMessage(std::string msg) {
	std::cout << msg << std::endl;
}

Vec2 NodeEditor::ScreenToWorld(Vec2 screenPoint) const {
	unsigned width, height;
	m_graphicsEngine->GetScreenSize(width, height);
	Vec2 screenSize = { width, height };
	Vec2 worldPoint = screenPoint / screenSize * m_camera->GetExtent() + m_camera->GetPosition() - m_camera->GetExtent()/2;
	return worldPoint;
}


int NodeEditor::FindNodeIndex(Node* node) const {
	int idx = -1;
	for (auto i : Range(m_nodes.size())) {
		if (node == m_nodes[i].get()) {
			idx = (int)i;
			return idx;
		}
	}
	assert(false);
	return idx;
}


} // namespace inl::tool