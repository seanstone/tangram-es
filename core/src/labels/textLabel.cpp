#include "labels/textLabel.h"

namespace Tangram {

TextLabel::TextLabel(std::string _text, Label::Transform _transform, Type _type, int _numGlyphs,
                     glm::vec2 _dim, TextBuffer& _mesh, size_t _bufferOffset)
    : Label(_transform, static_cast<LabelMesh&>(_mesh), _type, _bufferOffset, _numGlyphs * 4),
      m_text(_text) {
    m_dim = _dim;
}

void TextLabel::updateBBoxes() {
    glm::vec2 t = glm::vec2(cos(m_transform.state.rotation), sin(m_transform.state.rotation));
    glm::vec2 tperp = glm::vec2(-t.y, t.x);
    glm::vec2 obbCenter;

    obbCenter = m_transform.state.screenPos + t * m_dim.x * 0.5f - tperp * (m_dim.y / 8);

    m_obb = isect2d::OBB(obbCenter.x, obbCenter.y, m_transform.state.rotation, m_dim.x, m_dim.y);
    m_aabb = m_obb.getExtent();
}

}
