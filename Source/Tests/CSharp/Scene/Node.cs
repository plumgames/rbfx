// Copyright (c) 2024-2024 the rbfx project.
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT> or the accompanying LICENSE file.

using System;
using System.Threading.Tasks;
using Xunit;

namespace Urho3DNet.Tests
{
    public class NodeTests
    {
        [Fact]
        public async Task GetDerivedComponent_ByExactType()
        {
            await RbfxTestFramework.Context.ToMainThreadAsync();

            var node = RbfxTestFramework.Context.CreateObject<Node>();
            var component = node.CreateComponent<DerivedFromTestComponent>();

            Assert.Equal(component, node.FindComponent<DerivedFromTestComponent>(ComponentSearchFlag.Self | ComponentSearchFlag.Derived));
        }

        [Fact]
        public async Task GetDerivedComponent_ByBaseType()
        {
            await RbfxTestFramework.Context.ToMainThreadAsync();

            var node = RbfxTestFramework.Context.CreateObject<Node>();
            var component = node.CreateComponent<DerivedFromTestComponent>();

            Assert.Equal(component, node.FindComponent<Component>(ComponentSearchFlag.Self | ComponentSearchFlag.Derived));
        }

        [Fact]
        public async Task GetDerivedComponent_ByInterface()
        {
            await RbfxTestFramework.Context.ToMainThreadAsync();

            var node = RbfxTestFramework.Context.CreateObject<Node>();
            var component = node.CreateComponent<DerivedFromTestComponent>();

            Assert.Equal(component, (DerivedFromTestComponent)node.FindComponent<IDerivedFromTestComponent>(ComponentSearchFlag.Self | ComponentSearchFlag.Derived));
        }

        [Fact]
        public async Task GetParentDerivedComponent_ByInterface()
        {
            await RbfxTestFramework.Context.ToMainThreadAsync();

            var parent = RbfxTestFramework.Context.CreateObject<Node>();
            var node = parent.CreateChild("");
            var component = parent.CreateComponent<DerivedFromTestComponent>();

            Assert.Equal(component, (DerivedFromTestComponent)node.FindComponent<IDerivedFromTestComponent>(ComponentSearchFlag.ParentRecursive | ComponentSearchFlag.Derived));
            Assert.Null(parent.FindComponent<IDerivedFromTestComponent>(ComponentSearchFlag.ParentRecursive | ComponentSearchFlag.Derived));
        }
    }

    [DerivedFrom]
    public interface IDerivedFromTestComponent
    {
    }

    [ObjectFactory]
    public partial class DerivedFromTestComponent : Component, IDerivedFromTestComponent
    {
        public DerivedFromTestComponent(IntPtr cPtr, bool cMemoryOwn) : base(cPtr, cMemoryOwn)
        {
        }

        public DerivedFromTestComponent(Context context) : base(context)
        {
        }
    }
}
