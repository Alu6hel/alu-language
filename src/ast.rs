#[derive(Debug, PartialEq, Clone)]
pub enum AstNode {
    /// Represents the definition of a routine/function
    RoutineDefNode {
        name: String,
        body: Vec<AstNode>,
    },
    /// Represents accessing a capability pointer
    CapabilityAccessNode {
        base: String,
        offset: i64,
    },
    /// Represents standard binary operations (e.g. arithmetic, logic)
    BinaryOpNode {
        left: Box<AstNode>,
        op: String,
        right: Box<AstNode>,
    },
    /// Represents a basic number literal
    NumberLiteral(i64),
    /// Represents a macro call to be expanded
    MacroCallNode {
        name: String,
        args: Vec<AstNode>,
    },
}

pub struct MacroExpansionEngine;

impl MacroExpansionEngine {
    pub fn expand(node: AstNode) -> AstNode {
        match node {
            AstNode::MacroCallNode { name, args } => {
                if name == "UNROLL" {
                    // Very simple unroll macro simulation
                    let mut expanded_body = Vec::new();
                    for _ in 0..2 {
                        expanded_body.extend(args.clone());
                    }
                    AstNode::RoutineDefNode {
                        name: "unrolled".to_string(),
                        body: expanded_body,
                    }
                } else {
                    AstNode::MacroCallNode { name, args }
                }
            }
            AstNode::RoutineDefNode { name, body } => {
                let expanded_body = body.into_iter().map(Self::expand).collect();
                AstNode::RoutineDefNode { name, body: expanded_body }
            }
            AstNode::BinaryOpNode { left, op, right } => {
                AstNode::BinaryOpNode {
                    left: Box::new(Self::expand(*left)),
                    op,
                    right: Box::new(Self::expand(*right)),
                }
            }
            other => other,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_slice_3_ast_nodes() {
        // Constructing a routine definition containing a capability access and a binary operation
        let cap_access = AstNode::CapabilityAccessNode {
            base: "vga_buffer".to_string(),
            offset: 0,
        };
        
        let binary_op = AstNode::BinaryOpNode {
            left: Box::new(cap_access),
            op: "+".to_string(),
            right: Box::new(AstNode::NumberLiteral(1)),
        };

        let routine = AstNode::RoutineDefNode {
            name: "kernel_main".to_string(),
            body: vec![binary_op],
        };

        match routine {
            AstNode::RoutineDefNode { name, body } => {
                assert_eq!(name, "kernel_main");
                assert_eq!(body.len(), 1);
            }
            _ => panic!("Expected RoutineDefNode"),
        }
    }

    #[test]
    fn test_slice_4_macro_expansion() {
        let unroll_macro = AstNode::MacroCallNode {
            name: "UNROLL".to_string(),
            args: vec![AstNode::NumberLiteral(42)],
        };

        let expanded = MacroExpansionEngine::expand(unroll_macro);
        
        if let AstNode::RoutineDefNode { name, body } = expanded {
            assert_eq!(name, "unrolled");
            assert_eq!(body.len(), 2);
            assert_eq!(body[0], AstNode::NumberLiteral(42));
            assert_eq!(body[1], AstNode::NumberLiteral(42));
        } else {
            panic!("Macro failed to expand properly");
        }
    }
}
