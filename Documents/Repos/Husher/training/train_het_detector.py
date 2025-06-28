#!/usr/bin/env python3
"""
Train lightweight Hebrew ח detection model
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader, random_split
import numpy as np
import pandas as pd
from pathlib import Path
import argparse
import json
from sklearn.metrics import accuracy_score, precision_recall_fscore_support, confusion_matrix
import matplotlib.pyplot as plt
import seaborn as sns
from tqdm import tqdm
import onnx
import torch.onnx

class HebrewHetDataset(Dataset):
    """Dataset for Hebrew ח detection"""
    
    def __init__(self, features_path, labels_path, metadata_path=None):
        self.features = np.load(features_path)
        self.labels = np.load(labels_path)
        
        if metadata_path:
            self.metadata = pd.read_csv(metadata_path)
        else:
            self.metadata = None
        
        # Normalize features
        self.features = (self.features - np.mean(self.features, axis=0)) / (np.std(self.features, axis=0) + 1e-8)
        
        print(f"Dataset loaded: {len(self.features)} samples, {self.features.shape[1]} features")
        print(f"Class distribution: ח={np.sum(self.labels)}, Non-ח={len(self.labels) - np.sum(self.labels)}")
    
    def __len__(self):
        return len(self.features)
    
    def __getitem__(self, idx):
        return torch.FloatTensor(self.features[idx]), torch.LongTensor([self.labels[idx]])

class LightweightHetDetector(nn.Module):
    """Lightweight neural network for Hebrew ח detection"""
    
    def __init__(self, input_size=40, hidden_size=64, dropout=0.3):
        super(LightweightHetDetector, self).__init__()
        
        self.network = nn.Sequential(
            # First hidden layer
            nn.Linear(input_size, hidden_size),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.BatchNorm1d(hidden_size),
            
            # Second hidden layer
            nn.Linear(hidden_size, hidden_size // 2),
            nn.ReLU(),
            nn.Dropout(dropout),
            nn.BatchNorm1d(hidden_size // 2),
            
            # Third hidden layer
            nn.Linear(hidden_size // 2, hidden_size // 4),
            nn.ReLU(),
            nn.Dropout(dropout),
            
            # Output layer
            nn.Linear(hidden_size // 4, 2)  # Binary classification
        )
        
        # Initialize weights
        self.apply(self._init_weights)
    
    def _init_weights(self, module):
        if isinstance(module, nn.Linear):
            torch.nn.init.xavier_uniform_(module.weight)
            module.bias.data.fill_(0.01)
    
    def forward(self, x):
        return self.network(x)
    
    def predict_proba(self, x):
        """Get confidence scores"""
        with torch.no_grad():
            logits = self.forward(x)
            probabilities = torch.softmax(logits, dim=1)
            return probabilities[:, 1]  # Return probability of ח class

class ModelTrainer:
    """Training manager for Hebrew ח detection model"""
    
    def __init__(self, model, device='cpu'):
        self.model = model.to(device)
        self.device = device
        self.train_losses = []
        self.val_losses = []
        self.val_accuracies = []
    
    def train_epoch(self, dataloader, optimizer, criterion):
        """Train for one epoch"""
        self.model.train()
        total_loss = 0
        
        for features, labels in tqdm(dataloader, desc="Training"):
            features = features.to(self.device)
            labels = labels.squeeze().to(self.device)
            
            optimizer.zero_grad()
            outputs = self.model(features)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            
            total_loss += loss.item()
        
        return total_loss / len(dataloader)
    
    def validate(self, dataloader, criterion):
        """Validate model"""
        self.model.eval()
        total_loss = 0
        all_predictions = []
        all_labels = []
        
        with torch.no_grad():
            for features, labels in dataloader:
                features = features.to(self.device)
                labels = labels.squeeze().to(self.device)
                
                outputs = self.model(features)
                loss = criterion(outputs, labels)
                total_loss += loss.item()
                
                _, predicted = torch.max(outputs.data, 1)
                all_predictions.extend(predicted.cpu().numpy())
                all_labels.extend(labels.cpu().numpy())
        
        val_loss = total_loss / len(dataloader)
        accuracy = accuracy_score(all_labels, all_predictions)
        
        return val_loss, accuracy, all_predictions, all_labels
    
    def train(self, train_loader, val_loader, num_epochs=100, learning_rate=0.001):
        """Full training loop"""
        criterion = nn.CrossEntropyLoss()
        optimizer = optim.Adam(self.model.parameters(), lr=learning_rate, weight_decay=1e-5)
        scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=10, factor=0.5)
        
        best_val_accuracy = 0
        patience_counter = 0
        patience = 20
        
        print(f"🚀 Starting training for {num_epochs} epochs...")
        
        for epoch in range(num_epochs):
            # Train
            train_loss = self.train_epoch(train_loader, optimizer, criterion)
            
            # Validate
            val_loss, val_accuracy, _, _ = self.validate(val_loader, criterion)
            
            # Learning rate scheduling
            scheduler.step(val_loss)
            
            # Save metrics
            self.train_losses.append(train_loss)
            self.val_losses.append(val_loss)
            self.val_accuracies.append(val_accuracy)
            
            # Early stopping
            if val_accuracy > best_val_accuracy:
                best_val_accuracy = val_accuracy
                patience_counter = 0
                # Save best model
                torch.save(self.model.state_dict(), 'best_het_detector.pth')
            else:
                patience_counter += 1
            
            if epoch % 10 == 0 or patience_counter >= patience:
                print(f"Epoch {epoch:3d}: Train Loss={train_loss:.4f}, "
                      f"Val Loss={val_loss:.4f}, Val Acc={val_accuracy:.4f}")
            
            if patience_counter >= patience:
                print(f"Early stopping at epoch {epoch}")
                break
        
        print(f"✅ Training complete! Best validation accuracy: {best_val_accuracy:.4f}")
        return best_val_accuracy
    
    def evaluate(self, test_loader):
        """Comprehensive model evaluation"""
        print("📊 Evaluating model...")
        
        # Load best model
        self.model.load_state_dict(torch.load('best_het_detector.pth'))
        
        _, accuracy, predictions, labels = self.validate(test_loader, nn.CrossEntropyLoss())
        
        # Calculate detailed metrics
        precision, recall, f1, _ = precision_recall_fscore_support(labels, predictions, average='binary')
        
        print(f"Test Results:")
        print(f"  Accuracy:  {accuracy:.4f}")
        print(f"  Precision: {precision:.4f}")
        print(f"  Recall:    {recall:.4f}")
        print(f"  F1-Score:  {f1:.4f}")
        
        # Confusion matrix
        cm = confusion_matrix(labels, predictions)
        plt.figure(figsize=(8, 6))
        sns.heatmap(cm, annot=True, fmt='d', cmap='Blues',
                   xticklabels=['Non-ח', 'ח'], yticklabels=['Non-ח', 'ח'])
        plt.title('Confusion Matrix')
        plt.ylabel('True Label')
        plt.xlabel('Predicted Label')
        plt.savefig('confusion_matrix.png', dpi=300, bbox_inches='tight')
        plt.close()
        
        return {
            'accuracy': accuracy,
            'precision': precision,
            'recall': recall,
            'f1_score': f1,
            'confusion_matrix': cm.tolist()
        }
    
    def plot_training_history(self):
        """Plot training curves"""
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 5))
        
        # Loss curves
        ax1.plot(self.train_losses, label='Training Loss')
        ax1.plot(self.val_losses, label='Validation Loss')
        ax1.set_title('Training and Validation Loss')
        ax1.set_xlabel('Epoch')
        ax1.set_ylabel('Loss')
        ax1.legend()
        ax1.grid(True)
        
        # Accuracy curve
        ax2.plot(self.val_accuracies, label='Validation Accuracy')
        ax2.set_title('Validation Accuracy')
        ax2.set_xlabel('Epoch')
        ax2.set_ylabel('Accuracy')
        ax2.legend()
        ax2.grid(True)
        
        plt.tight_layout()
        plt.savefig('training_history.png', dpi=300, bbox_inches='tight')
        plt.close()

def export_to_onnx(model, input_size=40, output_path='het_detector.onnx'):
    """Export trained model to ONNX format"""
    print(f"📦 Exporting model to ONNX: {output_path}")
    
    # Load best model
    model.load_state_dict(torch.load('best_het_detector.pth'))
    model.eval()
    
    # Create dummy input
    dummy_input = torch.randn(1, input_size)
    
    # Export to ONNX
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        export_params=True,
        opset_version=11,
        do_constant_folding=True,
        input_names=['audio_features'],
        output_names=['het_probability'],
        dynamic_axes={
            'audio_features': {0: 'batch_size'},
            'het_probability': {0: 'batch_size'}
        }
    )
    
    # Verify ONNX model
    onnx_model = onnx.load(output_path)
    onnx.checker.check_model(onnx_model)
    
    print(f"✅ ONNX model saved: {output_path}")
    print(f"📏 Model size: {Path(output_path).stat().st_size / 1024:.1f} KB")

def main():
    parser = argparse.ArgumentParser(description='Train Hebrew ח detection model')
    parser.add_argument('--data-dir', type=str, default='./data/processed',
                       help='Directory containing processed data')
    parser.add_argument('--epochs', type=int, default=100,
                       help='Number of training epochs')
    parser.add_argument('--batch-size', type=int, default=64,
                       help='Batch size for training')
    parser.add_argument('--learning-rate', type=float, default=0.001,
                       help='Learning rate')
    parser.add_argument('--hidden-size', type=int, default=64,
                       help='Hidden layer size')
    parser.add_argument('--output-dir', type=str, default='./models',
                       help='Directory to save trained models')
    
    args = parser.parse_args()
    
    # Setup output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Load dataset
    data_dir = Path(args.data_dir)
    dataset = HebrewHetDataset(
        data_dir / "features.npy",
        data_dir / "labels.npy",
        data_dir / "metadata.csv"
    )
    
    # Split dataset
    train_size = int(0.7 * len(dataset))
    val_size = int(0.15 * len(dataset))
    test_size = len(dataset) - train_size - val_size
    
    train_dataset, val_dataset, test_dataset = random_split(
        dataset, [train_size, val_size, test_size]
    )
    
    # Create data loaders
    train_loader = DataLoader(train_dataset, batch_size=args.batch_size, shuffle=True)
    val_loader = DataLoader(val_dataset, batch_size=args.batch_size, shuffle=False)
    test_loader = DataLoader(test_dataset, batch_size=args.batch_size, shuffle=False)
    
    print(f"📊 Dataset splits: Train={train_size}, Val={val_size}, Test={test_size}")
    
    # Initialize model
    input_size = dataset.features.shape[1]
    model = LightweightHetDetector(input_size=input_size, hidden_size=args.hidden_size)
    
    # Check for GPU
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    print(f"🔧 Using device: {device}")
    
    # Train model
    trainer = ModelTrainer(model, device)
    best_accuracy = trainer.train(
        train_loader, val_loader, 
        num_epochs=args.epochs, 
        learning_rate=args.learning_rate
    )
    
    # Evaluate model
    test_metrics = trainer.evaluate(test_loader)
    
    # Plot training history
    trainer.plot_training_history()
    
    # Export to ONNX
    export_to_onnx(model, input_size, output_dir / "het_detector.onnx")
    
    # Save training configuration and results
    config = {
        'model_config': {
            'input_size': input_size,
            'hidden_size': args.hidden_size,
            'dropout': 0.3
        },
        'training_config': {
            'epochs': args.epochs,
            'batch_size': args.batch_size,
            'learning_rate': args.learning_rate
        },
        'results': test_metrics,
        'best_val_accuracy': best_accuracy
    }
    
    with open(output_dir / "training_config.json", 'w') as f:
        json.dump(config, f, indent=2)
    
    print("🎉 Training pipeline complete!")
    print(f"📁 Models saved in: {output_dir}")
    print(f"🎯 Final test accuracy: {test_metrics['accuracy']:.4f}")

if __name__ == "__main__":
    main()